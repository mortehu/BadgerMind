<?
$script = explode("\n", file_get_contents($path));
$instances = array();

foreach ($script as $line)
{
  $line = explode("\t", rtrim($line));

  if (sizeof($line) < 5)
    continue;

  $instances[] = $line;
}

function instance_compare($lhs, $rhs)
{
  /* Shader */
  if ($lhs[1] != $rhs[1])
    return strcmp($lhs[1], $rhs[1]);

  /* Model */
  if ($lhs[0] != $rhs[0])
    return strcmp($lhs[0], $rhs[0]);

  return 0;
}

usort($instances, 'instance_compare');

$command = "/usr/local/bin/bm-script-convert";

if ($best_format == "application/vnd.badgermind.sd.binary64.0")
  $command .= " --pointer-size=64";

$p = popen($command, "w");

foreach ($instances as $instance)
{
  fprintf ($p, "(instance model:(model URI:\"%s\" shader:(shader name:\"%s\")) transform:(mat4x4 x:%.6f y:%.6f z:%.6f))\n",
           $instance[0], $instance[1], $instance[2], $instance[3], $instance[4]);
}

fflush($p);
