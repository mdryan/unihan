<pre>
<?php
ini_set('max_execution_time', 300); // extend maximum execution time to five minutes. Probably better would be to process file in batches...
if(!$fh = fopen('unihan.txt', 'r')) {
    die('Unable to open unihan.txt, please check your installation and reinstall module.');
}
fgets($fh);   // throw away first line (may have BOM)
while(!feof($fh)) {
    $line = trim(fgets($fh));
    if($line[0] == "#" || $line=="") {
        continue; // throw away comments or blank lines
    }
    list($code, $field, $data) = explode("\t", $line);
    $code = mb_convert_encoding("&#".hexdec(substr($code,2)).";" , 'UTF-8', 'HTML-ENTITIES');
    if(in_array($field, array(
        'kBigFive', 'kCCCII', 'kCNS1986', 'kCNS1992', 'kEACC', 'kIBMJapan', 'kIRG_GSource',
        'kIRG_HSource', 'kIRG_JSource', 'kIRG_KSource', 'kIRG_KPSource', 'kIRG_TSource',
        'kIRG_USource', 'kIRG_VSource', 'kKPS0', 'kKPS1'))) {
        $data = hexdec($data);
    }
    if(!is_numeric($data)) {
        $data = "'" . db_escape_string($data) . "'";
    }
    $values[$code][$field] = $data;

if(!$seen[$code]) {
  db_query("INSERT INTO {unihan} SET `code`='$code'");
  $seen[$code] = true;
}


db_query("UPDATE {unihan} SET `$field`=$data WHERE `code`='$code'");
}
?>
</pre>
