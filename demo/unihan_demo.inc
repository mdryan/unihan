<?php
/**
* Main page listing all demonstrations
* @return string html page content
*/
function unihan_demo_page() {
  $items = unihan_demo_menu();
  $output = "<dl>";
  foreach ($items as $path => $item) {
    if ($path == 'unihan') continue;
    if ($item['type'] == MENU_CALLBACK) continue;
    if ($item['type'] == MENU_DEFAULT_LOCAL_TASK) continue;
    if (strpos($path, '%') !== FALSE) continue;
    if ($item['access callback'] === TRUE || user_access($item['access arguments'][0])) {
      $output .= "<dt>" . l(t($item['title']), $path) . "</dt>";
      $output .= "<dd>" . t($item['description']) . "</dd>";
    }
  }
  $output .= "</dl>";
  return $output;
}

function unihan_demo_pinyin_form($form_state) {
  return array(
    'convert' => array('#type' => 'textfield', '#title' => t('Chinese characters')),
    'submit' => array('#type' => 'submit', '#value' => t('Convert')),
  );
}

function unihan_demo_pinyin_form_submit($form, &$form_state) {
  global $base_url;
  $convert = $form_state['values']['convert'];
  $result = xmlrpc("$base_url/xmlrpc.php", 'unihan.pinyin', $convert, TRUE);
  drupal_set_message('<pre>' . print_r($result, TRUE) . '</pre>');
  $result = xmlrpc("$base_url/xmlrpc.php", 'unihan.pinyin', $convert, FALSE);
  drupal_set_message('<pre>' . print_r($result, TRUE) . '</pre>');
}
