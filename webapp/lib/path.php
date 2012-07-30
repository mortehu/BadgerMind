<?
$root_path = "/home/mortehu/BadgerMind-root/";

$path = explode("?", $_SERVER['REQUEST_URI'], 2);

if (sizeof($path) > 1)
  $params = $path[1];

$path = ltrim(preg_replace('/\/\/\/*/', '/', $path[0]), '/');

$relative_path = $path;

$path = "{$root_path}{$path}";

if (in_array('..', explode('/', $path)))
{
  header('HTTP/1.1 403 Forbidden');

  echo "403 Forbidden\n";

  exit;
}
