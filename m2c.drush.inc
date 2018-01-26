<?php

/**
 * @file
 * Drush make to Composer (m2c)
 */

/**
 * Implements hook_drush_help().
 */
function m2c_drush_help($section) {
  switch($section) {
    case 'drush:m2c':
      return dt("Drush make to Composer file conversion");
    break;
  }
}

/**
 * Implements hook_drush_command().
 */
function m2c_drush_command() {
  $items = array();
  $items['m2c'] = array(
    'description' => 'Convert a Drush makefile to composer format, on standard output',
    'arguments' => array(
      'makefile' => 'Makefile location',
      'composerfile' => 'File to write output it, allows for composer merge plugin',
    ),
    'options' => array(
      'add-drupal-repositories' => 'Add drupal project repositories (defaults to FALSE)',
      'require-dev' => 'Changes require into require-dev and places provide section (defaults to FALSE)',
    ),
    'aliases' => array('make2composer'),

    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  return $items;
}

/**
 * Implements hook_command_validate().
 *
 * Check makefile location provided, and that it exists.
 */
function drush_m2c_validate($makefile = NULL) {
  if (!$makefile) {
    drush_set_error("DRUSH_M2C_NO_MAKEFILE", "You must specify a makefile.");
    return;
  }
  if (!file_exists($makefile)) {
    drush_set_error("DRUSH_M2C_NO_MAKEFILE", "Makefile does not exist: '$makefile'.");
    return;
  }
}

/**
 * Drush command callback.
 */
function drush_m2c($makefile, $composerfile = NULL) {
  drush_include_engine("parsers", "default");

  $composer = m2c_parsers_default_m2c(
    m2c_parsers_default_internal($makefile)
  );

  // Allow for development json file.
  $require_string = "require";
  if (drush_get_option('require-dev')) {
    $require_string = "require-dev";
  }

  if (!isset($composerfile)) {
    $json = json_encode($composer, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . "\n";
    // Replace tabs with spaces.
    $json = preg_replace("~\\t~", ' ', $json);
    print $json;
  }
  else {
    if (isset($composer['repositories']) || isset($composer['extra']['patches'])) {
      // Provide folder for required includes.
      drush_mkdir("composer");
      // Add merge plugin.
      $composer['require'] = array('wikimedia/composer-merge-plugin' => 'dev-master') + $composer['require'];
      // Add merge plugin section.
      $composer['extra']['merge-plugin']['require'][] = "composer/*.json";

      // Split off repositories and add to merge array.
      if (count($composer['repositories']) > 1) {
        $repos = array(
          'repositories' => $composer['repositories']
        );

        unset($composer['repositories']);
        $composer['repositories'][] = array_shift($repos['repositories']);

        $repos = json_encode($repos, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . "\n";
        // Replace tabs with spaces.
        $repos = preg_replace("~\\t~", ' ', $repos);
        file_put_contents("composer/repositories.json", $repos);
      }

      if (count($composer['require']) > 3) {

        $require = array(
          $require_string => array_slice($composer['require'], 3, NULL, true),
        );

        // Copy all except core to provide section right after require section.
        if ($require_string == "require-dev") {
          unset($require['require-dev']['drupal/drupal']);
          $position = array_search('require', array_keys($composer)) + 1;
          $composer = array_slice($composer, 0, $position, true) + array("provide" => $require['require-dev']) + array_slice($composer, $position, count($composer) - $position, true);
        }

        $composer['require'] = array_slice($composer['require'], 0, 3, true);

        $require = json_encode($require, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . "\n";
        // Replace tabs with spaces.
        $require = preg_replace("~\\t~", ' ', $require);
        file_put_contents("composer/" . $require_string . ".json", $require);
      }

      // Split off repositories and add to merge array.
      if (isset($composer['extra']['patches'])) {
        $composer['extra']['merge-plugin']['merge-extra'] = true;

        $patches = array(
          'extra' => array(
            'patches' => $composer['extra']['patches'],
          ),
        );
        $patches = json_encode($patches, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . "\n";
        $repos = preg_replace("~\\t~", ' ', $repos);
        file_put_contents("composer/patches.json", $patches);

        unset($composer['extra']['patches']);
      }

      // Print remaining to main composer.json.
      $json = json_encode($composer, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . "\n";
      // Replace tabs with spaces.
      $json = preg_replace("~\\t~", ' ', $json);
      file_put_contents("composer.json", $json);
    }
  }
}

/**
 * Implements hook_drush_engine_type_info().
 */
function m2c_drush_engine_type_info() {
  return array(
    'hacks' => array(),
    'parsers' => array(),
  );
}

/**
 * Implements hook_drush_engine_ENGINE().
 */
function m2c_drush_engine_hacks() {
  return array(
    'make' => array(
      'description' => "Hacks",
    ),
  );
}


/**
 * Implements hook_drush_engine_ENGINE().
 */
function m2c_drush_engine_parsers() {
  return array(
    'default' => array(
      'description' => "Parsers",
    ),
  );
}
