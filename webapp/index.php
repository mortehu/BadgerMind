<?
date_default_timezone_set('GMT');

$method = $_SERVER['REQUEST_METHOD'];

if (isset($_SERVER['HTTP_DEPTH']))
  $depth = $_SERVER['HTTP_DEPTH'];
else
  $depth = 'infinity';

if ($method == 'OPTIONS')
{
  header ('Allow: OPTIONS, GET, HEAD, DELETE, PROPFIND, PUT, PROPPATCH, COPY, MOVE, REPORT, LOCK, UNLOCK');
  header ('MS-Author-Via: DAV');
  header ('Accept-Ranges: bytes');

  exit;
}

header ('DAV: 1, 3, extended-mkcol, 2');

require_once('lib/path.php');

function print_file_response($fullpath, $uri)
{
  $basename = pathinfo($fullpath, PATHINFO_BASENAME);

  if (is_dir($fullpath))
  {
    $fullpath = rtrim($fullpath, '/') . '/';
    $uri = rtrim($uri, '/') . '/';
  }

  echo "<d:response>";

  echo "<d:href>";
  echo $uri;
  echo "</d:href>";

  echo '<d:propstat>';
  echo '<d:prop>';

  if (false !== ($mtime = filemtime($fullpath)))
  {
    echo "<d:getlastmodified xmlns:b=\"urn:uuid:c2f41010-65b3-11d1-a29f-00aa00c14882/\" b:dt=\"dateTime.rfc1123\">";
    echo strftime('%a, %d %b %Y %T %Z', $mtime);
    echo "</d:getlastmodified>";
  }

  if (is_dir($fullpath))
  {
    echo "<d:resourcetype><d:collection/></d:resourcetype>";
    echo "<d:quota-used-bytes>100507529216</d:quota-used-bytes>";
    echo "<d:quota-available-bytes>49287102464</d:quota-available-bytes>";
  }
  else
  {
    echo "<d:resourcetype/>";

    if (false !== ($size = filesize($fullpath)))
    {
      echo "<d:getcontentlength>";
      echo $size;
      echo "</d:getcontentlength>";
    }

    echo "<d:getetag>";
    echo sha1(file_get_contents($fullpath));
    echo "</d:getetag>";
  }

  echo '</d:prop>';

  echo '<d:status>HTTP/1.1 200 OK</d:status>';
  echo '</d:propstat>';

  echo "</d:response>";
}

if (is_dir($path))
{
  $path = rtrim($path, '/') . '/';

  if ($method == 'MKCOL')
  {
    header('HTTP/1.1 405 Directory Already Exists');

    exit;
  }
  else
  {
    $files = scandir($path);

    if ($method == 'GET' || $method == 'HEAD')
    {
      ?>
      <!DOCTYPE html>
      <head>
        <title>Contents of <?=htmlentities($path, ENT_QUOTES, 'utf-8')?></title>
        <style>
          body { font-family: sans-serif; }
          .directory-listing { list-style: none; margin: 0; padding: 0; }
          .directory-listing img { margin-right: 5px; }
          .directory-listing li { margin-bottom: 2px; }
          .directory-listing a { text-decoration: none; }
          .directory-listing a:hover { text-decoration: underline; }
        </style>
      <body>
      <ul class="directory-listing">
      <?

      foreach ($files as $file)
      {
        if ($file[0] == '.')
          continue;

        $escaped_name = htmlentities($file, ENT_QUOTES, 'utf-8');

        $full_path = "$path/$file";
        $icon = '/famfamfam/page.png';

        $extension = strtolower(pathinfo($full_path, PATHINFO_EXTENSION));

        if (is_dir($full_path))
        {
          $icon = '/famfamfam/folder.png';
          $escaped_name .= '/';
        }
        else if ($extension == 'png')
          $icon = '/famfamfam/image.png';
        else if ($extension == 'fbx')
          $icon = '/famfamfam/bricks.png';


        ?><li><a href='<?=$escaped_name?>'><img src='<?=$icon?>' alt=''><?=$escaped_name?></a></li><?
      }

      ?></ul><?
    }
    else if ($method == 'PROPFIND')
    {
      $propfind = file_get_contents('php://input');

      header('HTTP/1.1 207 Multi-Status');
      header('Content-Type: application/xml; charset=utf-8');

      ob_start("ob_gzhandler");

      $base_uri = rtrim("{$_SERVER['REQUEST_URI']}", '/') . '/';

      echo "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
      echo "<d:multistatus xmlns:d=\"DAV:\">";

      print_file_response("$path", "$base_uri");

      if ($depth != '0')
      {
        foreach ($files as $file)
        {
          if ($file[0] == '.')
            continue;

          print_file_response("$path$file", "$base_uri$file");
        }
      }

      echo "</d:multistatus>\n";
    }

    exit;
  }
}
else if (file_exists($path))
{
  if ($method == 'PROPFIND')
  {
    $propfind = file_get_contents('php://input');

    header('HTTP/1.1 207 Multi-Status');
    header('Content-Type: application/xml; charset=UTF-8');

    ob_start("ob_gzhandler");

    echo "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    echo "<d:multistatus xmlns:d=\"DAV:\" xmlns:b=\"urn:uuid:c2f41010-65b3-11d1-a29f-00aa00c14882/\">";

    $base_uri = rtrim("{$_SERVER['REQUEST_URI']}", '/');

    print_file_response("$path", "$base_uri");

    echo "</d:multistatus>\n";

    exit;
  }
}

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

if (!file_exists($path))
{
  header('HTTP/1.1 404 Not Found');

  echo "404 Not Found\n";

  exit;
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

$conversions =
  array('application/vnd.autodesk.fbx application/vnd.badgermind.sd.binary.0' => '/usr/local/bin/bm-fbx-convert',
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
