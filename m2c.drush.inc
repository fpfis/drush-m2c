<?php

use Symfony\Component\Finder\Finder;

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
      'composerfile' => 'Location of a composer.json to save output into.',
    ),
    'options' => array(
      'name' => 'The package name (vendor/name).',
      'type' => 'The package type (drupal-profile).',
      'add-drupal-repositories' => 'Add drupal project repositories (defaults to FALSE)',
      'custom' => 'Provide path(s) where custom module, libraries and/or themes can be found. Multiple paths should be comma separated.',
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
function drush_m2c_validate($makefile = NULL, $composerfile = NULL) {
  if (!$makefile) {
    drush_set_error("DRUSH_M2C_NO_MAKEFILE", "You must specify a makefile.");
    return;
  }
  if (!file_exists($makefile)) {
    drush_set_error("DRUSH_M2C_NO_MAKEFILE", "Makefile does not exist: '$makefile'.");
    return;
  }
  if (file_exists($composerfile)) {
    $composer_original = file_get_contents($composerfile);
    $composer_original = json_decode($composer_original, true);
    if (isset($composer_original['name']) || !empty(drush_get_option('name'))) {
      drush_set_option('name', $composer_original['name']);
    }
    else {
      drush_set_error("DRUSH_M2C_NO_PROJECT_NAME", "You must specify a project name.");
      return;
    }
    if (isset($composer_original['type']) && empty(drush_get_option('type'))) {
      drush_set_option('type', $composer_original['type']);
    }
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

  // Create, update and add custom modules, themes and/or libraries.
  $custom = drush_get_option('custom');
  $custom = explode(",", $custom);
  foreach ($custom as $path) {
    if (!empty($path) && file_exists($path)) {
      $custom_composer = m2c_drush_create_custom_modules($path);
      $composer = array_merge_recursive($composer, $custom_composer);
    }
  }

  if (!isset($composerfile)) {
    $json = json_encode($composer, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . "\n";
    // Replace tabs with spaces.
    $json = preg_replace("~\\t~", ' ', $json);
    print $json;
  }
  else {
    if (isset($composer['repositories']) || isset($composer['extra']['patches'])) {
      $location = str_replace(".json", "", $composerfile);
      drush_mkdir($location);
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
        file_put_contents(rtrim($location, "/") . "/repositories.json", $repos);
      }

      if (count($composer['require']) > 3) {

        $require = array(
          $require_string => array_slice($composer['require'], 3, NULL, true),
        );

        // Copy all except core to provide section right after require section.
        if ($require_string == "require-dev") {
          if (isset($require['require-dev']['drupal/drupal'])) {
            $core = $require['require-dev']['drupal/drupal'];
            unset($require['require-dev']['drupal/drupal']);
          }
          $position = array_search('require', array_keys($composer)) + 1;
          $composer = array_slice($composer, 0, $position, true) + array("provide" => $require['require-dev']) + array_slice($composer, $position, count($composer) - $position, true);
          $composer['require'] = array_slice($composer['require'], 0, 3, true);
          if (isset($core)) {
            $composer['require']['drupal/drupal'] = $core;
          }
        }

        $require = json_encode($require, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . "\n";
        // Replace tabs with spaces.
        $require = preg_replace("~\\t~", ' ', $require);
        file_put_contents(rtrim($location, "/") . "/" . $require_string . ".json", $require);
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
        file_put_contents(rtrim($location, "/") . "/patches.json", $patches);

        unset($composer['extra']['patches']);
      }

      // Print remaining to main composer.json.
      $json = json_encode($composer, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . "\n";
      // Replace tabs with spaces.
      $json = preg_replace("~\\t~", ' ', $json);
      file_put_contents($composerfile, $json);
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

/**
 * Helper function to place composer.json in each folder with .info file.
 */
function m2c_drush_create_custom_modules($custom)
{
  $custom_composer = array(
    'repositories' => array(),
    'require' => array(),
  );

  drush_log(dt(
    "Searching for custom modules in :l.",
    array(":l" => $custom)
  ), "ok");

  $finder = new Finder();
  $finder->files()
    ->name("*.info")
    ->in($custom)
    ->depth('1')
    ->followLinks();

  foreach ($finder as $file) {
    $vendor = basename($custom);
    $version = "1.0.0";
    $name = basename($file->getRelativePath());
    $composer_json = dirname($file->getRealPath()) . "/composer.json";
    $composer = array(
      'name' => $vendor . "/" . $name,
      'version' => '1.0.0',
    );

    if (!file_exists($composer_json)) {
      $composer = array(
        'name' => $vendor . "/" . $name,
        'version' => $version,
      );

      $composer = json_encode($composer, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . "\n";
      $composer = preg_replace("~\\t~", ' ', $composer);
      
      if (file_put_contents($composer_json, $composer)) {
        drush_log(dt("Created composer.json for !p.", array("!p" => $vendor .  "/" . $name)), "warning");
      }
    }
    else {
      $composer_current = file_get_contents($composer_json);
      $composer_current = json_decode($composer_current, true);
      $composer = array_merge($composer, $composer_current);
      $version = $composer['version'];
      $composer = json_encode($composer, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . "\n";
      $composer = preg_replace("~\\t~", ' ', $composer);

      if (file_put_contents($composer_json, $composer)) {
        drush_log(dt("Updated composer.json for !p.", array("!p" => $vendor .  "/" . $name)), "warning");
      }
    }
    $custom_composer['require'][$vendor . "/" . $name] = $version;
    $custom_composer['repositories'][] = array(
      'type' => 'path',
      'url' => $custom . "/" . $name,
    );
  }
  return $custom_composer;
}
