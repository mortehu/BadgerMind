var DRAW_currentTexture;
var DRAW_pMatrix = mat4.create ();
var DRAW_mvMatrix = mat4.create ();

function DRAW_SetupShaders ()
{
  var fragmentShader = DRAW_GetShaderFromElement ("shader-fs");
  var vertexShader = DRAW_GetShaderFromElement ("shader-vs");

  shaderProgram = gl.createProgram ();
  gl.attachShader (shaderProgram, vertexShader);
  gl.attachShader (shaderProgram, fragmentShader);
  gl.linkProgram (shaderProgram);

  if (!gl.getProgramParameter (shaderProgram, gl.LINK_STATUS))
    {
      alert ("Could not initialise shaders");

      return;
    }

  gl.useProgram (shaderProgram);

  shaderProgram.vertexPositionAttribute = gl.getAttribLocation (shaderProgram, "aVertexPosition");
  gl.enableVertexAttribArray (shaderProgram.vertexPositionAttribute);

  shaderProgram.textureCoordAttribute = gl.getAttribLocation (shaderProgram, "aTextureCoord");
  gl.enableVertexAttribArray (shaderProgram.textureCoordAttribute);

  shaderProgram.modelViewProjectionMatrixUniform = gl.getUniformLocation (shaderProgram, "uModelViewProjectionMatrix");

  gl.uniform1i (gl.getUniformLocation (shaderProgram, "uSampler"), 0);
}

function DRAW_GetShaderFromElement (id)
{
  var shader, shaderScript;
  var str = "", k;

  shaderScript = document.getElementById (id);

  if (!shaderScript)
    return null;

  k = shaderScript.firstChild;

  while (k)
    {
      if (k.nodeType == 3)
        str += k.textContent;

      k = k.nextSibling;
    }

  if (shaderScript.type == "x-shader/x-fragment")
    shader = gl.createShader (gl.FRAGMENT_SHADER);
  else if (shaderScript.type == "x-shader/x-vertex")
    shader = gl.createShader (gl.VERTEX_SHADER);
  else
    return null;

  gl.shaderSource (shader, str);
  gl.compileShader (shader);

  if (!gl.getShaderParameter (shader, gl.COMPILE_STATUS))
    {
      alert (gl.getShaderInfoLog (shader));

      return null;
    }

  return shader;
}

function DRAW_LoadTexture (url)
{
  result = gl.createTexture ();
  result.image = new Image ();
  result.image.src = url;
  result.image.onload = function () { DRAW_HandleLoadedTexture (result) };

  return result;
}

function DRAW_HandleLoadedTexture (texture)
{
  gl.bindTexture (gl.TEXTURE_2D, texture);
  gl.pixelStorei (gl.UNPACK_FLIP_Y_WEBGL, true);
  gl.texImage2D (gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, texture.image);
  gl.texParameteri (gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
  gl.texParameteri (gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
  gl.generateMipmap (gl.TEXTURE_2D);
}

function DRAW_Flush ()
{
  var modelViewProjection = mat4.create ();

  mat4.identity (DRAW_pMatrix);
  mat4.perspective (45, gl.viewportWidth / gl.viewportHeight, 0.1, 100.0, DRAW_pMatrix);

  mat4.identity (DRAW_mvMatrix);
  mat4.translate (DRAW_mvMatrix, [100.0 * Math.cos (elapsed), 1.0, 100.0 * Math.sin (elapsed)]);

  mat4.multiply(DRAW_mvMatrix, DRAW_pMatrix, modelViewProjection);

  gl.uniformMatrix4fv (shaderProgram.modelViewProjectionMatrixUniform, false, modelViewProjection);

  gl.activeTexture (gl.TEXTURE0);
  gl.bindTexture (gl.TEXTURE_2D, DRAW_currentTexture);

  gl.bindBuffer (gl.ARRAY_BUFFER, vertexPositionBuffer);
  gl.vertexAttribPointer (shaderProgram.vertexPositionAttribute, 3, gl.FLOAT, false, 5 * 4, 0);
  gl.vertexAttribPointer (shaderProgram.textureCoordAttribute, 2, gl.FLOAT, false, 5 * 4, 3 * 4);

  gl.bindBuffer (gl.ELEMENT_ARRAY_BUFFER, vertexIndexBuffer);

  gl.drawElements (gl.TRIANGLES, indexCount, gl.UNSIGNED_SHORT, 0);

}

function DRAW_LoadModel (data)
{
  console.log ("Load model");

  for (var i = 0; i < data.length; ++i)
  {
    console.log(data);

    if (data[i]["texture-URI"])
      DRAW_currentTexture = DRAW_LoadTexture (data[i]["texture-URI"]);

    vertexPositionBuffer = gl.createBuffer ();
    gl.bindBuffer (gl.ARRAY_BUFFER, vertexPositionBuffer);
    gl.bufferData (gl.ARRAY_BUFFER, new Float32Array (data[i].vertices), gl.STATIC_DRAW);

    vertexIndexBuffer = gl.createBuffer ();
    gl.bindBuffer (gl.ELEMENT_ARRAY_BUFFER, vertexIndexBuffer);
    gl.bufferData (gl.ELEMENT_ARRAY_BUFFER, new Int16Array (data[i].triangles), gl.STATIC_DRAW);

    indexCount = data[i].triangles.length;
    /*
    vertexPositionBuffer.itemSize = 2;
    */
    break;
  }
}

/***********************************************************************/

var SYS_keys = {};

function SYS_Init ()
{
  var canvas = document.getElementById ("game-canvas");

  if (!(gl = WebGLUtils.setupWebGL (canvas)))
    return;

  document.onkeydown = function (event) { SYS_keys[String.fromCharCode (event.keyCode)] = true; };
  document.onkeyup = function (event) { SYS_keys[String.fromCharCode (event.keyCode)] = false; };

  gl.viewportWidth = canvas.width;
  gl.viewportHeight = canvas.height;

  gl.viewport (0, 0, gl.viewportWidth, gl.viewportHeight);

  DRAW_SetupShaders ();

  gl.clearColor (0.3, 0.0, 0.0, 1.0);
  gl.enable (gl.DEPTH_TEST);

  gl.enable (gl.BLEND);
  gl.blendFunc (gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA)
}

/***********************************************************************/

var lastTime = 0;
var elapsed = 0;

function VIEWER_SetupTextures ()
{
  placeholder = DRAW_LoadTexture ("placeholder.png");
}

function VIEWER_Init (modelURI)
{
  var httpreq = new XMLHttpRequest();

  SYS_Init ();

  httpreq.open('GET', modelURI, true);

  httpreq.onreadystatechange =
    function ()
    {
      if (httpreq.readyState == 4)
      {
        DRAW_LoadModel (eval(httpreq.responseText));

        VIEWER_Update ();
      }
    }

  httpreq.send(null);
}

function VIEWER_Update ()
{
  var timeNow, deltaTime;

  timeNow = new Date ().getTime ();
  deltaTime = timeNow - lastTime;

  if (deltaTime < 0 || deltaTime > 0.05)
    deltaTime = 0.05;

  gl.clear (gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

  if (SYS_keys['W']) y -= deltaTime * 10.0;
  if (SYS_keys['A']) x -= deltaTime * 10.0;
  if (SYS_keys['S']) y += deltaTime * 10.0;
  if (SYS_keys['D']) x += deltaTime * 10.0;

  DRAW_Flush ();

  elapsed += deltaTime;

  requestAnimFrame (VIEWER_Update);
}
