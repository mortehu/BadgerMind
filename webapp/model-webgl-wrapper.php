<!DOCTYPE html>
<head>
  <title><?=pathinfo($_SERVER['REQUEST_URI'], PATHINFO_BASENAME)?> (Badgermind model viewer)</title>
  <script src='/webgl-utils.js'></script>
  <script src='/gl-matrix/gl-matrix-min.js'></script>
  <script src='/webgl-viewer.js'></script>
  <style>
    body { background: #111; margin: 0; padding: 0; font-family: Arial, sans-serif; }
  </style>
  <script id='shader-fs' type='x-shader/x-fragment'>
    precision mediump float;

    uniform sampler2D uSampler;

    varying vec2 vTextureCoord;

    void main()
    {
      gl_FragColor = vec4(1.0,1.0,1.0,1.0);/*texture2D (uSampler, vec2 (vTextureCoord.s, vTextureCoord.t));*/
    }
  </script>
  <script id='shader-vs' type='x-shader/x-vertex'>
    attribute vec3 aVertexPosition;
    attribute vec2 aTextureCoord;

    uniform mat4 uModelViewProjectionMatrix;

    varying vec2 vTextureCoord;

    void main (void)
    {
      gl_Position = uModelViewProjectionMatrix * vec4(aVertexPosition, 1.0);
      gl_Position.y -= 0.00005 * gl_Position.w * gl_Position.w; /* Horizon */
      vTextureCoord = aTextureCoord;
    }
  </script>
<body onload='VIEWER_Init("<?=$_SERVER['REQUEST_URI']?>?media-type=application/json")'>
  <canvas id='game-canvas' width=800 height=600 style='width: 800px; height: 600px; border: 2px solid #555; display: block; margin: 20px auto'></canvas>
