<?
function git_root($path)
{
  chdir($path);

  $lines = array();

  exec('/usr/bin/git rev-parse --show-toplevel', $lines);

  return $lines[0];
}

function git_status($path)
{
  chdir($path);

  $lines = array();
  $result = array();

  exec('/usr/bin/git status --ignored --porcelain .', $lines);

  foreach($lines as $line)
  {
    $filename = substr($line, 3);
    $result[$filename] = substr($line, 0, 2);
  }

  return $result;
}

function git_delete($git_root, $path)
{
  chdir($git_root);

  exec('/usr/bin/git rm -- ' . escapeshellarg($path));
}

function git_commit($git_root, $paths, $message, $author)
{
  $message_path = tempnam(sys_get_temp_dir(), "commit");
  file_put_contents($message_path, $message);

  $paths = array_map('escapeshellarg', $paths);
  $author = escapeshellarg($author);
  
  exec('/usr/bin/git commit --file=' . escapeshellarg($message_path) .
                          ' --author=' . escapeshellarg($author) . 
                          ' -- ' . join(' ', $paths));

  unlink($message_path);
}

function git_add($git_root, $path)
{
  chdir($git_root);

  exec('/usr/bin/git add -- ' . escapeshellarg($path));
}

function git_status_explain($xy)
{
  switch ($xy)
  {
  case ' M': return 'modified';
  case '??': return 'untracked';
  case '!!': return 'ignored';
  default:   return $xy;
  }
}
