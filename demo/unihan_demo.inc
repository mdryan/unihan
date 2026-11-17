<?php

/**
 * @file
 * Page callbacks for the unihan demonstration module.
 */

/**
 * Page callback: lists all demonstrations.
 */
function unihan_demo_page() {
  $output = '<dl>';
  foreach (unihan_demo_menu() as $path => $definition) {
    if ($path == 'unihan' || strpos($path, '%') !== FALSE) {
      continue;
    }
    // menu_get_item() checks access and returns translated text.
    $item = menu_get_item($path);
    $hidden_types = [MENU_CALLBACK, MENU_DEFAULT_LOCAL_TASK];
    if (!$item || !$item['access'] || in_array($item['type'], $hidden_types)) {
      continue;
    }
    $output .= '<dt>' . l($item['title'], $path) . '</dt>';
    $output .= '<dd>' . filter_xss_admin($item['description']) . '</dd>';
  }
  $output .= '</dl>';
  return $output;
}

/**
 * Form constructor for the pinyin conversion demonstration.
 *
 * @see unihan_demo_pinyin_submit()
 */
function unihan_demo_pinyin($form, &$form_state) {
  $form['convert'] = [
    '#type' => 'textfield',
    '#title' => t('Chinese characters'),
    '#required' => TRUE,
  ];
  $form['context'] = [
    '#type' => 'radios',
    '#title' => t('Convert as'),
    '#options' => [
      '' => t('Text'),
      'surname' => t('Family name'),
      'given' => t('Given name'),
    ],
    '#default_value' => '',
    '#description' => t('Family and given names use the name readings of characters where these differ, e.g. 曾 is read zēng as a family name.'),
  ];
  $form['submit'] = ['#type' => 'submit', '#value' => t('Convert')];
  if (!empty($form_state['storage']['rows'])) {
    $form['result'] = [
      '#theme' => 'table',
      '#rows' => $form_state['storage']['rows'],
    ];
  }
  return $form;
}

/**
 * Form submission handler for unihan_demo_pinyin().
 */
function unihan_demo_pinyin_submit($form, &$form_state) {
  $convert = $form_state['values']['convert'];
  $context = $form_state['values']['context'] ? $form_state['values']['context'] : NULL;
  $rows = [];
  foreach ([TRUE => t('With tone marks'), FALSE => t('Without tone marks')] as $tone_marks => $label) {
    $result = unihan_word2pinyin($convert, (bool) $tone_marks, $context);
    $rows[] = [$label, check_plain($result['pinyin'])];
  }
  $form_state['storage']['rows'] = $rows;
  $form_state['rebuild'] = TRUE;
}
