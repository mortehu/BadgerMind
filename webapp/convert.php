<?
date_default_timezone_set('CET');

$path = explode("?", $_SERVER['REQUEST_URI'], 2);

if (sizeof($path) > 1)
  $params = $path[1];

$path = ltrim($path[0], '/');

if ($path[0] == '.' || preg_match('/\.php$/', $path) || preg_match('/\.\./', $path))
{
  header('HTTP/1.1 403 Forbidden');

  echo "403 Forbidden\n";

  exit;
}

$conversions =
  array('application/vnd.autodesk.fbx application/vnd.badgermind.m0' => '/usr/local/bin/bm-fbx-convert');

if ($path[strlen($path) - 1] == '/')
  $path .= 'scene.bsd';

if (file_exists($path))
{
  $extension = pathinfo($path, PATHINFO_EXTENSION);

  if (0 == strcasecmp($extension, "fbx"))
    $content_type = 'application/vnd.autodesk.fbx';
  elseif (0 == strcasecmp($extension, "png"))
    $content_type = 'image/png';
  elseif (0 == strcasecmp($extension, "jpg"))
    $content_type = 'image/jpeg';
  elseif (0 == strcasecmp($extension, "mp3"))
    $content_type = 'audio/mpeg';
  elseif (0 == strcasecmp($extension, "mp4"))
    $content_type = 'audio/mp4';
  elseif (0 == strcasecmp($extension, "webm"))
    $content_type = 'audio/webm';
  elseif (0 == strcasecmp($extension, "bsd"))
    $content_type = 'application/vnd.badgermind.sd';
}

if (!isset($content_type))
{
  header('HTTP/1.1 404 Not Found');

  echo "404 Not Found\n";

  exit;
}

 if(isset($_SERVER['HTTP_IF_MODIFIED_SINCE']))
 {
   $if_modified_since = strtotime(preg_replace('/;.*$/', '', $_SERVER['HTTP_IF_MODIFIED_SINCE']));

   if (filemtime($path) <= $if_modified_since)
   {
     header('HTTP/1.1 304 Not Modified');

     exit;
   }
 }

if (isset($_SERVER['HTTP_ACCEPT']) && sizeof($_SERVER['HTTP_ACCEPT']))
{
  $best_quality = 0;

  $accept = array();

  foreach (explode(',', $_SERVER['HTTP_ACCEPT']) as $header)
  {
    $tmp = explode(';q=', $header, 2);

    $accept[$tmp[0]] = (sizeof($tmp) < 2) ? 1.0 : $tmp[1];
  }

  arsort($accept);

  foreach ($accept as $format => $q)
  {
    if ($q <= $best_quality)
      continue;

    if ($format == '*/*' || $format == $content_type)
    {
      $best_format = $format;
      $best_quality = $q;
      unset($best_handler);
    }
    elseif (isset($conversions["$content_type $format"]))
    {
      $best_format = $format;
      $best_quality = $q;
      $best_handler = $conversions["$content_type $format"];
    }
  }

  if (!isset($best_format))
  {
    header('HTTP/1.1 406 Not Acceptable');

    echo "406 Not Acceptable\n";

    exit;
  }
}
else
  $best_format = $content_type;

header("Vary: Accept");
header("Last-Modified: " . strftime("%a, %d %b %Y %T %z", filemtime($path)));

if (!isset($best_handler))
{
  header("Content-Type: $content_type");
  header("Content-Length: " . filesize($path));

  readfile($path);
}
else
{
  header("Content-Type: $content_type");

  passthru("$best_handler " . escapeshellarg($path));
}