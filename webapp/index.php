<?
date_default_timezone_set('CET');

$method = $_SERVER['REQUEST_METHOD'];

if (in_array(strtoupper($method), array('PUT', 'POST', 'MKCOL', 'DELETE')))
{
  if (!isset($_SERVER['PHP_AUTH_USER']))
  {
    header('WWW-Authenticate: Basic realm="BadgerMind"');
    header('HTTP/1.0 401 Unauthorized');
    echo 'Mutating operations require authentication';

    exit;
  }
}

$path = explode("?", $_SERVER['REQUEST_URI'], 2);

if (sizeof($path) > 1)
  $params = $path[1];

$path = ltrim(preg_replace('/\/\/\/*/', '/', $path[0]), '/');

if (!preg_match('/\//', $path) || $path[0] == '.' || preg_match('/\.php$/', $path) || preg_match('/\.\./', $path))
{
  header('HTTP/1.1 403 Forbidden');

  echo "403 Forbidden\n";

  exit;
}

if (is_dir($path))
{
  $path = rtrim($path, '/') . '/';

  if ($method == 'MKCOL')
  {
    header('HTTP/1.1 405 Directory Already Exists');

    exit;
  }
  else if (file_exists("$path/scene.bsd") && $method != 'PROPFIND')
  {
    $path = "$path/scene.bsd";
  }
  else
  {
    $files = scandir($path);

    if ($method == 'GET' || $method == 'HEAD')
    {
      ?><ul><?

      foreach ($files as $file)
      {
        if ($file[0] == '.')
          continue;

        $escaped_name = htmlentities($file, ENT_QUOTES, 'utf-8');

        ?><li><a href='<?=$escaped_name?>'><?=$escaped_name?></a></li><?
      }

      ?></ul><?
    }
    else if ($method == 'PROPFIND')
    {
      $propfind = simplexml_load_file('php://input');

      header('HTTP/1.1 207 Multi-Status');
      header('Content-Type: text/xml');

      $base_uri = rtrim("http://{$_SERVER['HTTP_HOST']}{$_SERVER['REQUEST_URI']}", '/') . '/';

      echo "<?xml version='1.0' encoding='utf-8'?>\n";
      echo "<multistatus xmlns='DAV:'>\n";

        echo "<response>";

        echo "<href>";
        echo $base_uri;
        echo "</href>";

        echo '<propstat>';
        echo '<status>HTTP/1.1 200 OK</status>';
        echo '<prop>';
        echo '<resourcetype><collection/></resourcetype>';
        echo '</prop>';
        echo '</propstat>';

        echo "</response>\n";

      foreach ($files as $file)
      {
        if ($file[0] == '.')
          continue;

        $fullpath = $base_uri . $file;

        if (is_dir($fullpath))
          $fullpath .= '/';

        $localpath = $path . $file;

        echo "<response>";

        echo "<href>";
        echo $fullpath;
        echo "</href>";

        echo '<propstat>';
        echo '<prop>';

        if (false !== ($mtime = filemtime($localpath)))
        {
          echo "<getlastmodified>";
          echo strftime('%a, %d %b %y %T %z', $mtime);
          echo "</getlastmodified>";
        }

        if (is_dir($localpath))
        {
          echo "<resourcetype><collection/></resourcetype>";
        }
        else
        {
          if (false !== ($size = filesize($localpath)))
          {
            echo "<getcontentlength>";
            echo $size;
            echo "</getcontentlength>";
          }
        }

        echo '</prop>';

        echo '<status>HTTP/1.1 200 OK</status>';
        echo '</propstat>';

        echo "</response>\n";
      }

      echo "</multistatus>\n";
    }

    exit;
  }
}

if ($method == 'OPTIONS')
  exit;

if ($method == 'MKCOL')
{
  if (false !== mkdir($path, 0777))
    header('HTTP/1.1 201 Created');
  else
    header('HTTP/1.1 500 mkdir failed for some reason');

  exit;
}

if ($method == 'PUT')
{
  $output = fopen("php://input", "r");

  if (false === ($input = fopen($path, "w")))
  {
    header("HTTP/1.1 500 Failed to open target path for writing");

    exit;
  }
  else
  {
    while ($data = fread($output, 65536))
    {
      if (false === fwrite($input, $data))
      {
        header("HTTP/1.1 500 Error writing to target file");

        exit;
      }
    }

    fclose($input);
    fclose($output);

    header('HTTP/1.1 200 OK');

    exit;
  }
}

if ($method != 'GET' && $method != 'HEAD')
{
  header("HTTP/1.1 405 Method {$method} Not Allowed");
  echo "405 Method {$method} Not Allowed";

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
  else
  {
    readfile($path);

    exit;
  }
}
else
{
  header('HTTP/1.1 404 Not Found');

  echo "404 Not Found\n";

  exit;
}

$conversions =
  array('application/vnd.autodesk.fbx application/vnd.badgermind.m0' => '/usr/local/bin/bm-fbx-convert',
        'application/vnd.autodesk.fbx application/json' => '/usr/local/bin/bm-fbx-convert --format=json',
        'application/vnd.autodesk.fbx text/html' => 'model-webgl-wrapper.php',
        'application/vnd.badgermind.sd application/vnd.badgermind.sd.binary.0' => '/usr/local/bin/bm-script-convert',
        'application/vnd.badgermind.sd text/html' => '/usr/local/bin/bm-script-convert --format=html');

if (isset($_GET['media-type']) && isset($conversions["$content_type {$_GET['media-type']}"]))
{
  $best_format = $_GET['media-type'];
  $best_quality = 1.0;
  $best_handler = $conversions["$content_type {$_GET['media-type']}"];
}
else if (isset($_SERVER['HTTP_ACCEPT']) && sizeof($_SERVER['HTTP_ACCEPT']))
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
      $best_format = $content_type;
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
  header("Content-Type: $best_format");
  header("Content-Length: " . filesize($path));

  readfile($path);
}
else
{
  header("Content-Type: $best_format");

  if ($best_handler[0] == '/')
    passthru("$best_handler " . escapeshellarg($path));
  else
    require($best_handler);
}
