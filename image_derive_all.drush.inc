<?php

/**
 * @file
 * Implementation for the image-derive-all drush command.
 *
 * Generate image derivatives for given image styles.
 */

use Drupal\Core\Database\Database;
use Drupal\Core\Database\Query\Condition;
use Drupal\image\Entity\ImageStyle;

/**
 * Implements hook_drush_command().
 */
function image_derive_all_drush_command() {
  $items = [];

  $items['image-derive-all'] = [
    'description' => "Create image derivatives.",
    'arguments' => [
      'styles' => 'A comma delimited list of image style machine name(s).',
    ],
    'options' => [
      'exclude' => [
        'description' => 'A comma delimited list of all image styles to be excluded.',
        'example_value' => 'large',
      ],
      'dir' => [
        'description' => 'Images derivates will be generated of all images inside this directory (relative to public files directory). Default will generate derivates of all public images.',
        'example_value' => 'xyz',
      ],
      'purge' => [
        'description' => 'Regenerate existing image derivatives.',
      ],
    ],
    'examples' => [
      'drush image-derive-all' => 'Generates image derivatives for every image style of every image in the public file system.',
      'drush image-derive-all thumbnail,teaser --dir=xyz' => 'Generates image derivatives for the image style "thumbnail" and "teaser" of images in the "xyz" directory in the public file directory.',
      'drush image-derive-all --exclude=thumbnail --dir=public' => 'Generates image derivatives for all image styles except "thumbnail" for every file within the public file system directory but not in subdirectories.',
      'drush image-derive-all thumbnail --dir=public --purge' => 'Generates image derivatives for the "thumbnail" style only and overwriting images if they exist, for every file within the public file system directory but not in subdirectories.',
    ],
    'core' => ['8', '9'],
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
    'callback' => 'image_derive_all_drush_action',
    'aliases' => ['ida'],
  ];

  return $items;
}

/**
 * Command callback. Create image derivatives for give styles.
 */
function image_derive_all_drush_action() {

  $purge = drush_get_option('purge', FALSE);

  $files = image_derive_all_get_relevant_files(drush_get_option('dir'));

  $excludes = explode(",", drush_get_option('exclude'));
  $includes = explode(",", func_get_args()[0]);

  $imageStyles = ImageStyle::loadMultiple();

  /* @var string $name Name of Style */
  /* @var ImageStyle $style ImageStyle Object */
  foreach ($imageStyles as $name => $style) {
    if (!empty($excludes) && in_array($name, $excludes)) {
      drush_print('Excluding ' . $name);
      continue;
    }
    if (empty($includes[0]) || in_array($name, $includes)) {

      drush_print("Processing Style $name");
      $count = 1;
      foreach ($files as $file) {
        $destination = $style->buildUri($file->uri);
        if ($purge) {
          if (file_exists($destination)) {
            unlink($destination);
          }
        }
        if (!file_exists($destination)) {
          $style->createDerivative($file->uri, $destination);
          image_derive_all_show_progress($count, count($files), $name);
        }
        $count++;
      }
      drush_print("Style $name Processed");
    }
  }
}

/**
 * Fetch files from database.
 */
function image_derive_all_get_relevant_files($directory) {
  $extensions = ['jpeg', 'jpg', 'gif', 'png'];
  $mimetypes = ['image/jpeg', 'image/jpg', 'image/gif', 'image/png'];

  $dir = rtrim($directory, '/');
  if ($dir == 'public') {
    // Finds anything that does not contain "/", should be fine.
    $file_pattern = "[^\/]*";
  }
  else {
    $file_pattern = $dir ? $dir . ".+" : ".+";
  }

  $regex = "^public:\/\/(" . $file_pattern . ")\.(" . implode('|', $extensions) . ")$";

  // Query the database for files that match this pattern.
  $query = Database::getConnection()
    ->select('file_managed', 'f');
  $andGroup = $query->andConditionGroup()
    ->condition('filemime', $mimetypes, 'IN')
    ->condition('uri', $regex, 'REGEXP');
  $query->condition($andGroup);

  $total_count = $query
    ->countQuery()
    ->execute()
    ->fetchField();

  drush_print(\Drupal::translation()->formatPlural($total_count,
    '1 entry is to have its image derivatives created.',
    '@count entries are to have their image derivatives created.')
  );

  // Select the files to have derivatives created.
  return $query->fields('f', array('fid', 'filename', 'uri'))
    ->execute()
    ->fetchAll();
}

/**
 * Provide progress info.
 */
function image_derive_all_show_progress($count, $total, $name) {
  if (!isset($display)) {
    static $display = [];
  }

  if (!array_key_exists($name, $display)) {
    // Integer steps to display <= 100.
    $display[$name] = range(25, 100, 25);
  }
  $progress = ($count / $total) * 100;

  if (count($display[$name]) and $progress >= $display[$name][0]) {
    drush_print("Style $name progress " . $display[$name][0] . "%");
    array_shift($display[$name]);
  }
}
