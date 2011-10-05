<?php

/**
 * Returns path to unihan.txt file or FALSE if not found.
 */
function unihan_admin_get_filepath() {
  static $filepath;
  if (isset($filepath)) {
    return $filepath;
  }
  $path = FALSE;
  // Libraries API integration.
  if (function_exists('libraries_get_path')) {
    if ($path = libraries_get_path('unihan')) {
      if (file_exists($path . '/unihan.txt')) {
        $path .= '/unihan.txt';
      }
    }
  }
  // Manually check sites/all/libraries in case Libraries API is not available.
  elseif (file_exists('./sites/all/libraries/unihan/unihan.txt')) {
    $path = 'sites/all/libraries/unihan/unihan.txt';
  }
  return $path;
}

/**
 * Admin page overview
 */
function unihan_admin() {
  return l('Import data', 'admin/settings/unihan/import');
}

/**
 * Import unihan.txt into database
 */
function unihan_admin_import() {
  $batch = array(
    'title' => t('Importing unihan.txt'),
    'operations' => array(),
    'progress_message' => '',
    'finished' => '_unihan_admin_import_finished',
    'file' => drupal_get_path('module', 'unihan').'/unihan.admin.inc',
  );
  if (!$fh = @fopen(unihan_admin_get_filepath(), 'r')) {
    drupal_set_message(t('Unable to open unihan.txt, please check your installation.'), 'error');
    return '';
  }
  fgets($fh); // throw away first line (always a comment and maybe BOM)
  while (!feof($fh)) {
    $pos = ftell($fh);
    $batch['operations'][] = array('_unihan_admin_import_from', array($pos));
    $count = 0;
    while (!feof($fh) && $count++ < 500) {
      fgets($fh);
    }
  }
  fclose($fh);
  batch_set($batch);
  batch_process('admin/settings/unihan');
}

/**
 * Batch callback
 */
 function _unihan_admin_import_from($pos, &$context) {
  $context['message'] = t('Imported !size', array('!size' => format_size($pos)));
  if (!$fh = @fopen(unihan_admin_get_filepath(), 'r')) {
    if (!$context['results']['error']) {
      drupal_set_message(t('Unable to open unihan.txt, please check your installation.'), 'error');
    }
    $context['results']['error'] = TRUE;
    return;
  }
  fseek($fh, $pos);
  while (!feof($fh) && $count++ < 500) {
    $line = trim(fgets($fh));
    if($line == '' || $line[0] == '#') {
      continue;
    }
    list($char, $field, $data) = explode("\t", $line);
    $code = html_entity_decode('&#' . hexdec($char) . ';', ENT_NOQUOTES, 'UTF-8');
    if (!db_result(db_query("SELECT COUNT(*) FROM {unihan} WHERE code='%s'", $code))) {
      db_query("INSERT INTO {unihan} SET `code`='%s'", $code);
    }
    if (in_array($field, array(
      'kBigFive', 'kCCCII', 'kCNS1986', 'kCNS1992', 'kEACC', 'kIBMJapan', 'kIRG_GSource',
      'kIRG_HSource', 'kIRG_JSource', 'kIRG_KSource', 'kIRG_KPSource', 'kIRG_TSource',
      'kIRG_USource', 'kIRG_VSource', 'kKPS0', 'kKPS1'))) {
      $data = hexdec($data);
    }
    if (!is_numeric($data)) {
      $data = "'" . db_escape_string($data) . "'";
    }
    db_query("UPDATE {unihan} SET `$field`=$data WHERE code='%s'", $code);
  }
  fclose($fh);
}

/**
 * Batch completion callback
 */
function _unihan_admin_import_finished($success, $results, $operations) {
  if ($results['error']) {
    drupal_set_message(t('Import did not complete.'), 'error');
  }
  else {
    variable_set('unihan_imported', TRUE);
    drupal_set_message(t('Import of unihan data completed'));
  }
}
