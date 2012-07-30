<?
$script = explode("\n", file_get_contents($path));
$instances = array();
$i = 0;

foreach ($script as $line)
{
  $line = explode("\t", rtrim($line));

  if (sizeof($line) == 5)
    $instances[$i++] = $line;
}

function find_models($prefix, $path)
{
  $result = array();
  $entries = scandir($path);

  foreach ($entries as $entry)
  {
    if ($entry[0] == '.')
      continue;

    $full_path = "$path/$entry";

    if (is_dir($full_path))
    {
      $result = array_merge($result, find_models("$prefix$entry/", $full_path));

      continue;
    }

    $extension = pathinfo($entry, PATHINFO_EXTENSION);

    if (0 == strcasecmp($extension, "fbx"))
    {
      $result[] = "$prefix$entry";
    }
  }

  return $result;
}

function to_csv($array)
{
  return join("\t", $array) . "\n";
}

function commit_changes()
{
  global $path;
  global $instances;

    header('Content-Type: text/plain');

  $tmpname = tempnam(pathinfo($path, PATHINFO_DIRNAME), "bid.tmp.");

  if (false !== file_put_contents($tmpname, join("", array_map('to_csv', $instances))))
  {
    header('HTTP/1.1 303 See Other');
    header("Location: http://{$_SERVER['HTTP_HOST']}{$_SERVER['REQUEST_URI']}");
    rename($tmpname, $path);
    
    exit;
  }
  else
  {
    header('HTTP/1.1 500 Internal Server Error');
    header('Content-Type: text/plain');

    echo "Could not create updated instance file";

    @unlink($tmpname);
  }
}

if (isset($_POST['delete']))
{
  unset($instances[$_POST['delete']]);
  $instances = array_values($instances);

  commit_changes();
}

if (isset($_POST['add']))
{
  if (!is_numeric($_POST['x']) || !is_numeric($_POST['y']) || !is_numeric($_POST['z']))
  {
    header('HTTP/1.0 400 Bad Request');
    header('Content-Type: text/plain');

    echo "Invalid X/Y/Z coordinates.  Must be numeric";

    exit;
  }

  $new_instance = array($_POST['model'], $_POST['shader'], $_POST['x'], $_POST['y'], $_POST['z']);
  $instances[] = $new_instance;

  commit_changes();

  exit;
}

$models = find_models("", pathinfo($path, PATHINFO_DIRNAME));
natcasesort($models);

$shaders = array('Scenery', 'BoneAnim');
?>
<!DOCTYPE html>
<head>
  <title><?=pathinfo($_SERVER['REQUEST_URI'], PATHINFO_BASENAME)?> (Badgermind instance viewer)</title>
  <style>
    body { font-family: Arial, sans-serif; }
    table { border-spacing: 0; border-collapse: collapse; }
    th, td { padding: 2px 5px; border: 1px solid #ccc; text-align: left; }
    .n { text-align: right; }
    .f { border: none; }
  </style>
<body>
  <table>
    <tr>
      <th>Model
      <th>Shader
      <th class='n'>X
      <th class='n'>Y
      <th class='n'>Z
  <? foreach ($instances as $k => $instance): ?>
    <tr>
      <td><a href='<?=html($instance[0])?>'><?=html($instance[0])?></a>
      <td><?=html($instance[1])?>
      <td class='n'><?=number_format($instance[2], 3, '.', '')?>
      <td class='n'><?=number_format($instance[3], 3, '.', '')?>
      <td class='n'><?=number_format($instance[4], 3, '.', '')?>
      <td class='f'><form method='post' action='<?=html($_SERVER['REQUEST_URI'])?>'><input type='image' name='delete' value='<?=$k?>' src='/famfamfam/delete.png' alt='Delete'></form>
  <? endforeach ?>
    <form method='post' action='<?=html($_SERVER['REQUEST_URI'])?>'>
    <tr>
      <td>
        <select name='model'>
          <? foreach ($models as $model): ?>
            <option><?=html($model)?></option>
          <? endforeach ?>
        </select>
      <td>
        <select name='shader'>
          <? foreach ($shaders as $shader): ?>
            <option><?=html($shader)?></option>
          <? endforeach ?>
        </select>
      <td class='n'><input type='number' name='x' value='0' size='5'>
      <td class='n'><input type='number' name='y' value='0' size='5'>
      <td class='n'><input type='number' name='z' value='0' size='5'>
      <td class='f'><input type='image' name='add' value='<?=$k?>' src='/famfamfam/add.png' alt='Add'>
    </form>
  </table>
