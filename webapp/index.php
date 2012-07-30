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
require_once('lib/git.php');

$email_addresses = array('mortehu' => 'Morten Hustveit <morten.hustveit@gmail.com>',
                         'flemm' => 'Tollef Roe Steen <tollef.steen@hihm.no>');

if (isset($_SERVER['REMOTE_USER'])
    && isset($email_addresses[$_SERVER['REMOTE_USER']]))
{
  $email_address = $email_addresses[$_SERVER['REMOTE_USER']];
}
else
  $email_address = "";

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

$git_root = git_root(pathinfo($path, PATHINFO_DIRNAME)) . '/';

if (strlen($git_root) > strlen($root_path)
    && !strncmp($root_path, $git_root, strlen($root_path)))
{
  $git_prefix = substr($path, strlen($git_root));
}
else
{
  unset($git_root);
}

if ($method == 'POST')
{
  switch ($_POST['action'])
  {
  case 'add':

    git_add($git_root, $git_prefix);

    header('HTTP/1.1 303 See Other');
    header("Location: http://{$_SERVER['HTTP_HOST']}/" . pathinfo($relative_path, PATHINFO_DIRNAME) . "/");

    break;

  case 'delete':

    git_delete($git_root, $git_prefix);

    header('HTTP/1.1 303 See Other');
    header("Location: http://{$_SERVER['HTTP_HOST']}/" . pathinfo($relative_path, PATHINFO_DIRNAME) . "/");

    break;

  case 'commit':

    if (strlen(trim($_POST['shortlog'])) < 3)
    {
      header ('Content-Type: text/plain');
      echo "You need to provide a meaningful shortlog";

      exit;
    }

    if (!preg_match('/.+ <.*@.*>$/', $_POST['author']))
    {
      header ('Content-Type: text/plain');
      echo "You must provide an author on the form Name <name@example.org>";

      exit;
    }

    if (strlen($_POST['details']))
      $message = trim($_POST['shortlog']) . "\n\n" . $_POST['details'];
    else
      $message = trim($_POST['shortlog']);

    git_commit($git_root, $_POST['paths'], $message, $_POST['author']);

    header('HTTP/1.1 303 See Other');
    header("Location: http://{$_SERVER['HTTP_HOST']}/" . pathinfo($relative_path, PATHINFO_DIRNAME) . "/");

    break;
  }

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
  else
  {
    $files = scandir($path);

    $pending_changes = array();

    if (isset($git_root))
    {
      $git_status = git_status($path);

      foreach ($git_status as $file => $gs)
      {
        if ($gs != '??' && $gs != '!!')
          $pending_changes[$file] = $gs;
      }
    }
    else
    {
      $git_status = array();
      $git_prefix = "";
    }

    if ($method == 'GET' || $method == 'HEAD')
    {
      ?>
      <!DOCTYPE html>
      <head>
        <title>Contents of <?=htmlentities($path, ENT_QUOTES, 'utf-8')?></title>
        <style>
          body { font-family: sans-serif; }
          .directory-listing { border-spacing: 0; }
          .directory-listing td { padding: 0; }
          .directory-listing a { text-decoration: none; }
          .directory-listing a:hover { text-decoration: underline; }
          .n { text-align: right; }
          td.text { padding: 0 5px; }
          ul.pending-changes { list-style: none; margin-left: 0; padding-left: 15px; }
          ul.pending-changes li { margin-left: 0; padding-left: 0; }
        </style>
      <body>
      <table class="directory-listing">
      <?

      if (sizeof (explode('/', $relative_path)) > 1)
      {
        ?>
        <tr>
          <td><img src='/famfamfam/folder.png' alt=''>
          <td class='text'><a href='..'>Parent Directory</a>
        <?
      }

      foreach ($files as $file)
      {
        if ($file[0] == '.')
          continue;

        $escaped_name = htmlentities($file, ENT_QUOTES, 'utf-8');

        $full_path = "$path/$file";
        $git_path = "{$git_prefix}{$file}";
        $icon = '/famfamfam/page.png';

        $extension = strtolower(pathinfo($full_path, PATHINFO_EXTENSION));

        if ($is_dir = is_dir($full_path))
        {
          $icon = '/famfamfam/folder.png';
          $escaped_name .= '/';
          $git_path .= '/';
        }
        else if ($extension == 'png')
          $icon = '/famfamfam/image.png';
        else if ($extension == 'fbx')
          $icon = '/famfamfam/bricks.png';

        if (array_key_exists($git_path, $git_status))
        {
          $gs = $git_status[$git_path];
          unset ($git_status[$git_path]);
        }
        else
          $gs = "";
        ?>
        <tr>
          <td><img src='<?=$icon?>' alt=''>
          <td class='text'><a href='<?=$escaped_name?>'><?=$escaped_name?></a>
          <td class='n text'><?=number_format(filesize($full_path))?>
            <?
            $can_add = false;
            $can_delete = false;
            $can_revert = false;
            $is_modified = false;
            $extra = "";

            switch ($gs)
            {
            case '??':

              $can_add = true;

              break;

            case false:

              if (!$is_dir)
                $can_delete = true;

              break;

            case ' M':

              $is_modified = true;
              $can_revert = true;

            case '!!':

              break;

            default:

              $extra = git_status_explain($gs);
            }
          ?>
          <td>
            <? if ($is_modified): ?>
              <img src='/famfamfam/page_edit.png' name='action' value='add' alt='Submit' title='File is modified'>
            <? endif ?>
          <td>
            <? if ($can_add): ?>
              <form method='post' action='<?=$escaped_name?>'>
                <input type='image' src='/famfamfam/page_add.png' name='action' value='add' alt='Submit' title='Add to version control'>
              </form>
            <? endif ?>
          <td>
            <? if ($can_delete): ?>
              <form method='post' action='<?=$escaped_name?>'>
                <input type='image' src='/famfamfam/page_delete.png' name='action' value='delete' alt='Submit' title='Delete'>
              </form>
            <? endif ?>
          <td>
            <? if ($can_revert): ?>
              <form method='post' action='<?=$escaped_name?>'>
                <input type='image' src='/famfamfam/page_refresh.png' name='action' value='revert' alt='Submit' title='Revert changes'>
              </form>
            <? endif ?>
          <td>
            <?=$extra?>
          <?
      }

      ?></table><?

      if (sizeof($pending_changes))
      {
        $icon_for_change = array('A' => '/famfamfam/page_add.png', 'D' => '/famfamfam/page_delete.png', ' ' => '/famfamfam/page_edit.png');
        ?>
        <h2>Commit</h2>
        <form action='<?=htmlentities($_SERVER['REQUEST_URI'], ENT_QUOTES, 'utf-8')?>' method='post'>
          <input type='hidden' name='action' value='commit'>
          <p>Pending Changes:</p>
          <ul class='pending-changes'>
          <? foreach ($pending_changes as $file => $gs): ?>
            <li><label><input type='checkbox' name='paths[]' value='<?=htmlentities($file, ENT_QUOTES, 'utf-8')?>'>
              <img src='<?=$icon_for_change[$gs[0]]?>'> <?=htmlentities($file, ENT_QUOTES, 'utf-8')?></li>
          <? endforeach ?>
          </ul>
          <p>Author:<br>
          <input type='text' name='author' size='50' value='<?=htmlentities($email_address, ENT_QUOTES, 'utf-8')?>'></p>
          <p>Commit Summary (max 50 characters, use present tense):<br>
          <input type='text' name='shortlog' size='50' maxlength='50'></p>
          <p>Commit Explanation:<br>
          <textarea name='details' rows='6' cols='72' wrap='hard'></textarea></p>
          <p><input type='submit' value='Commit'></p>
        </form>
        <?
      }
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
elseif (0 == strcasecmp($extension, "bid"))
  $content_type = 'application/vnd.badgermind.id';
else
{
  readfile($path);

  exit;
}

$conversions =
  array('application/vnd.autodesk.fbx application/vnd.badgermind.sd.binary.0' => '/usr/local/bin/bm-fbx-convert',
        'application/vnd.autodesk.fbx application/vnd.badgermind.sd.binary64.0' => '/usr/local/bin/bm-fbx-convert --pointer-size=64',
        'application/vnd.autodesk.fbx application/json' => '/usr/local/bin/bm-fbx-convert --format=json',
        'application/vnd.autodesk.fbx text/html' => 'model-webgl-wrapper.php',
        'application/vnd.badgermind.id text/plain' => 'show-as-plaintext.php',
        'application/vnd.badgermind.id application/vnd.badgermind.sd.binary.0' => 'bid-to-script.php',
        'application/vnd.badgermind.id application/vnd.badgermind.sd.binary64.0' => 'bid-to-script.php',
        'application/vnd.badgermind.id text/html' => 'bid-to-html.php',
        'application/vnd.badgermind.sd text/plain' => 'show-as-plaintext.php',
        'application/vnd.badgermind.sd application/vnd.badgermind.sd.binary.0' => '/usr/local/bin/bm-script-convert',
        'application/vnd.badgermind.sd application/vnd.badgermind.sd.binary64.0' => '/usr/local/bin/bm-script-convert --pointer-size=64',
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
