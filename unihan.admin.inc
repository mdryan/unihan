<?php

/**
 * @file
 * Administration pages and import batch for the unihan module.
 */

/**
 * Number of lines of unihan.txt processed per batch operation.
 */
define('UNIHAN_IMPORT_LINES_PER_OPERATION', 500);

/**
 * Returns path to unihan.txt file or FALSE if not found.
 */
function unihan_admin_get_filepath() {
  $filepath = &drupal_static(__FUNCTION__);
  if (isset($filepath)) {
    return $filepath;
  }
  $filepath = FALSE;
  // Libraries API integration.
  if (function_exists('libraries_get_path') && ($path = libraries_get_path('unihan'))) {
    $candidates = [$path . '/unihan.txt'];
  }
  // Manually check sites/all/libraries in case Libraries API is not available.
  else {
    $candidates = ['sites/all/libraries/unihan/unihan.txt'];
  }
  foreach ($candidates as $candidate) {
    if (is_readable($candidate)) {
      $filepath = $candidate;
      break;
    }
  }
  return $filepath;
}

/**
 * Page callback: Unihan database status page.
 */
function unihan_admin() {
  $filepath = unihan_admin_get_filepath();
  if ($filepath) {
    $file = check_plain($filepath) . ' (' . format_size(filesize($filepath)) . ')';
  }
  else {
    $file = t('Not found. Place unihan.txt in sites/all/libraries/unihan.');
  }
  $characters = db_query('SELECT COUNT(*) FROM {unihan}')->fetchField();
  $imported = variable_get('unihan_imported', FALSE);
  $rows = [
    [t('Data file'), $file],
    [t('Characters in database'), number_format($characters)],
    [t('Import status'), $imported ? t('Imported') : t('Not yet imported')],
  ];
  return [
    'status' => [
      '#theme' => 'table',
      '#rows' => $rows,
    ],
  ];
}

/**
 * Form constructor for confirming an import of unihan.txt.
 *
 * @see unihan_admin_import_submit()
 */
function unihan_admin_import($form, &$form_state) {
  $filepath = unihan_admin_get_filepath();
  if (!$filepath) {
    drupal_set_message(t('Unable to find unihan.txt, please check your installation.'), 'error');
    return $form;
  }
  return confirm_form($form,
    t('Import the Unihan database?'),
    'admin/config/regional/unihan',
    t('This imports %file (%size) into the {unihan} table, updating any characters already present. It can take several minutes.', [
      '%file' => $filepath,
      '%size' => format_size(filesize($filepath)),
    ]),
    t('Import'),
    t('Cancel')
  );
}

/**
 * Form submission handler for unihan_admin_import().
 *
 * Splits unihan.txt into chunks of lines and imports each chunk in a batch
 * operation.
 */
function unihan_admin_import_submit($form, &$form_state) {
  $filepath = unihan_admin_get_filepath();
  if (!$filepath || !($fh = @fopen($filepath, 'r'))) {
    drupal_set_message(t('Unable to open unihan.txt, please check your installation.'), 'error');
    return;
  }
  $batch = [
    'title' => t('Importing unihan.txt'),
    'operations' => [],
    'finished' => '_unihan_admin_import_finished',
    'file' => drupal_get_path('module', 'unihan') . '/unihan.admin.inc',
  ];
  // Throw away first line (always a comment and maybe a BOM).
  fgets($fh);
  while (!feof($fh)) {
    $batch['operations'][] = ['_unihan_admin_import_from', [ftell($fh)]];
    $count = 0;
    while (!feof($fh) && $count++ < UNIHAN_IMPORT_LINES_PER_OPERATION) {
      fgets($fh);
    }
  }
  fclose($fh);
  batch_set($batch);
  $form_state['redirect'] = 'admin/config/regional/unihan';
}

/**
 * Batch operation callback: imports a chunk of lines from unihan.txt.
 *
 * @param int $pos
 *   Byte offset in unihan.txt at which to start reading.
 * @param array $context
 *   Batch context.
 */
function _unihan_admin_import_from($pos, array &$context) {
  $context['message'] = t('Imported @size', ['@size' => format_size($pos)]);
  if (!empty($context['results']['error'])) {
    return;
  }
  $filepath = unihan_admin_get_filepath();
  if (!$filepath || !($fh = @fopen($filepath, 'r'))) {
    drupal_set_message(t('Unable to open unihan.txt, please check your installation.'), 'error');
    $context['results']['error'] = TRUE;
    return;
  }
  $schema = drupal_get_schema('unihan');
  fseek($fh, $pos);
  // Lines for each character are contiguous in unihan.txt, so collect all of
  // a character's fields and write them with a single query, and wrap the
  // whole chunk in a transaction to avoid a disk flush after every write.
  $transaction = db_transaction();
  $code = NULL;
  $fields = [];
  $count = 0;
  while (!feof($fh) && $count++ < UNIHAN_IMPORT_LINES_PER_OPERATION) {
    $line = trim(fgets($fh));
    if ($line == '' || $line[0] == '#') {
      continue;
    }
    $parts = explode("\t", $line, 3);
    if (count($parts) < 3) {
      continue;
    }
    list($char, $field, $data) = $parts;
    // Skip fields unknown to the schema (e.g. from newer unihan.txt versions).
    if ($field == 'code' || !isset($schema['fields'][$field])) {
      $context['results']['skipped_fields'][$field] = TRUE;
      continue;
    }
    $char_code = html_entity_decode('&#' . hexdec(substr($char, 2)) . ';', ENT_NOQUOTES, 'UTF-8');
    if ($char_code !== $code) {
      _unihan_admin_import_save($code, $fields, $context);
      $code = $char_code;
      $fields = [];
    }
    $fields[$field] = _unihan_admin_import_value($schema['fields'][$field], $field, $data);
  }
  _unihan_admin_import_save($code, $fields, $context);
  unset($transaction);
  fclose($fh);
}

/**
 * Saves the fields collected for one character.
 *
 * @param string|null $code
 *   The character, or NULL if no character has been read yet.
 * @param array $fields
 *   Values keyed by field name.
 * @param array $context
 *   Batch context.
 */
function _unihan_admin_import_save($code, array $fields, array &$context) {
  if ($code === NULL || !$fields) {
    return;
  }
  try {
    db_merge('unihan')
      ->key(['code' => $code])
      ->fields($fields)
      ->execute();
  }
  catch (PDOException $e) {
    // Typically a character outside the Basic Multilingual Plane, which a
    // utf8 (3-byte) MySQL table cannot store.
    if (!isset($context['results']['failed'])) {
      $context['results']['failed'] = 0;
    }
    $context['results']['failed']++;
  }
}

/**
 * Converts a unihan.txt value to suit the type of its database column.
 *
 * @param array $spec
 *   Schema field specification.
 * @param string $field
 *   Field name.
 * @param string $data
 *   Raw value from unihan.txt.
 *
 * @return mixed
 *   Value to store, or NULL if it can't be represented in the column.
 */
function _unihan_admin_import_value(array $spec, $field, $data) {
  $hex_fields = [
    'kBigFive', 'kCCCII', 'kCNS1986', 'kCNS1992', 'kEACC', 'kIBMJapan',
    'kIRG_GSource', 'kIRG_HSource', 'kIRG_JSource', 'kIRG_KSource',
    'kIRG_KPSource', 'kIRG_TSource', 'kIRG_USource', 'kIRG_VSource',
    'kKPS0', 'kKPS1',
  ];
  if (in_array($field, $hex_fields)) {
    $data = hexdec($data);
  }
  // Remove characters outside the Basic Multilingual Plane, which the utf8
  // (3-byte) {unihan} table cannot store and which would otherwise cause the
  // whole character's row to be rejected.
  $data = preg_replace('/[\x{10000}-\x{10FFFF}]/u', '', $data);
  switch ($spec['type']) {
    case 'int':
    case 'float':
    case 'numeric':
      // Use the first value where several are given; drop non-numeric values.
      list($first) = explode(' ', trim($data), 2);
      return is_numeric($first) ? $first : NULL;

    case 'varchar':
    case 'char':
      return drupal_substr($data, 0, $spec['length']);

    default:
      return $data;
  }
}

/**
 * Batch finished callback for the unihan.txt import.
 */
function _unihan_admin_import_finished($success, $results, $operations) {
  if (!$success || !empty($results['error'])) {
    drupal_set_message(t('Import did not complete.'), 'error');
    return;
  }
  variable_set('unihan_imported', TRUE);
  drupal_set_message(t('Import of unihan data completed.'));
  if (!empty($results['failed'])) {
    drupal_set_message(t('@count characters could not be stored, most likely because they are outside the Basic Multilingual Plane.', ['@count' => $results['failed']]), 'warning');
  }
  if (!empty($results['skipped_fields'])) {
    drupal_set_message(t('Fields not in the database schema were skipped: @fields', ['@fields' => implode(', ', array_keys($results['skipped_fields']))]), 'warning');
  }
}
