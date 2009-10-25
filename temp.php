<pre>
<?php
if(!$fh = fopen('unihan.txt', 'r')) {
  die('Unable to open unihan.txt, please check your installation and reinstall module.');
}
fgets($fh);   // throw away first line header (may have BOM)
while(!feof($fh)) {
    $line = trim(fgets($fh));
    if($line[0] == "#") {
        continue; // throw away comments
    }
    if($line=="") {
      continue; // throw away blank lines
    }
    list($code, $field, $data) = explode("\t", $line);
    if(strlen($data) > $fields[$field]['size']) {
        $fields[$field]['size'] = strlen($data);
    }
//    $fields[$field]['count']++;
    if(!is_numeric($data)) {
        $fields[$field]['string'] = true;
    }
    if(is_numeric($data) && strpos($data, ".")!==FALSE) {
        $fields[$field]['float'] = true;
    }
}
print "\$schema['unihan'] = array(\n";
print "  'fields' => array(\n";
print "    'code' => array('type' => 'char', 'length' => 1),\n";
foreach($fields as $field => $data) {
    if ($data['float']) {
        $type = "float";
        $length = 0;
    }
    else if ($data['string']) {
        if($data['size'] < 16) {
            $length = 32;
            $type = "varchar";
        }
        else if($data['size'] < 128) {
            $length = 255;
            $type = "varchar";
        }
        else {
            $type = "text";
            $length = "2048";
        }
    }
    else {
        $type = "float";
        $length = 0;
    }
    print "    '$field' => array('type' => '$type'";
    if($length) {
        print ", 'length' => $length";
    }
    print "),\n";
}
print "  ),\n";
print "  'primary key' => array('code'),\n";
print "  'indexes' => array('code' => array('code')),\n";
print ");\n";
?>
</pre>
