<?
require_once('lib/path.php');

if (false === ($mtime = filemtime($path)))
{
  header('HTTP/1.1 404 Not Found');

  echo '404 Not Found';

  exit;
}
?>
<!DOCTYPE html>
<head>
  <title><?=pathinfo($_SERVER['REQUEST_URI'], PATHINFO_BASENAME)?> (Badgermind model viewer)</title>
  <script src='/webgl-utils.js'></script>
  <script src='/gl-matrix/gl-matrix-min.js'></script>
  <script src='/webgl-viewer.js'></script>
  <style>
    body { background: #111; margin: 0; padding: 0; font-family: Arial, sans-serif; }
    #help { text-align: center; color: #aaa; }
  </style>
  <script id="shader-fs" type="x-shader/x-fragment">
  precision mediump float;
  varying vec2 vTextureCoord;

  uniform sampler2D uSampler;

  void main (void)
  {
    gl_FragColor = texture2D (uSampler, vec2 (vTextureCoord.s, vTextureCoord.t));
  }
  </script>
  <script id="shader-vs" type="x-shader/x-vertex">
  attribute vec3 aVertexPosition;
  attribute vec2 aTextureCoord;

  uniform mat4 uMVMatrix;
  uniform mat4 uPMatrix;

  varying vec2 vTextureCoord;

  void main (void)
  {
    gl_Position = uPMatrix * uMVMatrix * vec4 (aVertexPosition.x, aVertexPosition.y, aVertexPosition.z, 1.0);
    vTextureCoord = aTextureCoord;
  }
  </script>
  <script id="bone-vs" type="x-shader/x-vertex">
  attribute vec3 aVertexPosition;
  attribute vec2 aTextureCoord;
  attribute vec4 aBoneWeights;
  attribute vec4 aBoneIndices;

  uniform mat4 uMVMatrix;
  uniform mat4 uPMatrix;
  uniform vec4 uBones[80 * 3];

  varying vec2 vTextureCoord;

  void main (void)
  {
    mat4 mat;
    vec4 skinnedPosition;

    mat[0] =  uBones[int(aBoneIndices.x) * 3    ] * aBoneWeights.x;
    mat[1] =  uBones[int(aBoneIndices.x) * 3 + 1] * aBoneWeights.x;
    mat[2] =  uBones[int(aBoneIndices.x) * 3 + 2] * aBoneWeights.x;
    mat[0] += uBones[int(aBoneIndices.y) * 3    ] * aBoneWeights.y;
    mat[1] += uBones[int(aBoneIndices.y) * 3 + 1] * aBoneWeights.y;
    mat[2] += uBones[int(aBoneIndices.y) * 3 + 2] * aBoneWeights.y;
    mat[0] += uBones[int(aBoneIndices.z) * 3    ] * aBoneWeights.z;
    mat[1] += uBones[int(aBoneIndices.z) * 3 + 1] * aBoneWeights.z;
    mat[2] += uBones[int(aBoneIndices.z) * 3 + 2] * aBoneWeights.z;
    mat[0] += uBones[int(aBoneIndices.w) * 3    ] * aBoneWeights.w;
    mat[1] += uBones[int(aBoneIndices.w) * 3 + 1] * aBoneWeights.w;
    mat[2] += uBones[int(aBoneIndices.w) * 3 + 2] * aBoneWeights.w;
    mat[3] = vec4(mat[0].w, mat[1].w, mat[2].w, 1.0);
    mat[0].w = 0.0;
    mat[1].w = 0.0;
    mat[2].w = 0.0;

    skinnedPosition = mat * vec4(aVertexPosition, 1.0);

    gl_Position = uPMatrix * uMVMatrix * skinnedPosition;
    vTextureCoord = aTextureCoord;
  }
  </script>
<body onload='VIEWER_Init("<?=$_SERVER['REQUEST_URI']?>?media-type=application/json&amp;mtime=<?=$mtime?>&amp;v=15")'>
  <canvas id='game-canvas' width=800 height=600 style='width: 800px; height: 600px; border: 2px solid #555; display: block; margin: 20px auto'></canvas>
  <div style='text-align:center'><select id='take'></select></div>
  <div id='help'>Q/A = move closer/further away.  W/S = move up/down.  Yes, the controls are terrible.</div>
