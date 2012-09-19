var gl;
var distance = 1.0, up = 0.3;
var DRAW_currentTexture;
var DRAW_pMatrix = mat4.create ();
var DRAW_mvMatrix = mat4.create ();
var DRAW_modelMatrix = mat4.create ();

var takes = [];
var bindPose = [];
var minBounds = [ 0, 0, 0 ];
var maxBounds = [ 0, 0, 0 ];
var center = [ 0, 0, 0 ];

var currentAnim = 0;

var vertexPositionBuffer;
var vertexIndexBuffer;

var lastTime = 0;

var staticProgram;
var boneProgram;

function DRAW_SetupShaders ()
{
  var fragmentShader, vertexShader;

  fragmentShader = DRAW_GetShaderFromElement ("shader-fs");
  vertexShader = DRAW_GetShaderFromElement ("shader-vs");
  boneVSShader = DRAW_GetShaderFromElement ("bone-vs");

  staticProgram = gl.createProgram ();
  gl.attachShader (staticProgram, vertexShader);
  gl.attachShader (staticProgram, fragmentShader);
  gl.linkProgram (staticProgram);

  if (!gl.getProgramParameter (staticProgram, gl.LINK_STATUS))
    {
      console.log (gl.getProgramInfoLog(staticProgram));
      alert ("Could not initialise static shader: " + gl.getProgramInfoLog(staticProgram));

      return false;
    }

  gl.useProgram (staticProgram);

  staticProgram.vertexPositionAttribute = gl.getAttribLocation (staticProgram, "aVertexPosition");
  gl.enableVertexAttribArray (staticProgram.vertexPositionAttribute);

  staticProgram.textureCoordAttribute = gl.getAttribLocation (staticProgram, "aTextureCoord");
  gl.enableVertexAttribArray (staticProgram.textureCoordAttribute);

  staticProgram.pMatrixUniform = gl.getUniformLocation (staticProgram, "uPMatrix");
  staticProgram.mvMatrixUniform = gl.getUniformLocation (staticProgram, "uMVMatrix");

  gl.uniform1i (gl.getUniformLocation (staticProgram, "uSampler"), 0);

  boneProgram = gl.createProgram ();
  gl.attachShader (boneProgram, boneVSShader);
  gl.attachShader (boneProgram, fragmentShader);
  gl.linkProgram (boneProgram);

  if (!gl.getProgramParameter (boneProgram, gl.LINK_STATUS))
    {
      console.log (gl.getProgramInfoLog(boneProgram));
      alert ("Could not initialise bone shader: " + gl.getProgramInfoLog(boneProgram));

      return false;
    }

  gl.useProgram (boneProgram);

  boneProgram.vertexPositionAttribute = gl.getAttribLocation (boneProgram, "aVertexPosition");
  boneProgram.textureCoordAttribute = gl.getAttribLocation (boneProgram, "aTextureCoord");
  boneProgram.boneWeightAttribute = gl.getAttribLocation (boneProgram, "aBoneWeights");
  boneProgram.boneIndexAttribute = gl.getAttribLocation (boneProgram, "aBoneIndices");

  gl.enableVertexAttribArray (boneProgram.vertexPositionAttribute);
  gl.enableVertexAttribArray (boneProgram.textureCoordAttribute);
  gl.enableVertexAttribArray (boneProgram.boneWeightAttribute);
  gl.enableVertexAttribArray (boneProgram.boneIndexAttribute);

  boneProgram.bonesUniform = gl.getUniformLocation (boneProgram, "uBones");
  boneProgram.pMatrixUniform = gl.getUniformLocation (boneProgram, "uPMatrix");
  boneProgram.mvMatrixUniform = gl.getUniformLocation (boneProgram, "uMVMatrix");

  gl.uniform1i (gl.getUniformLocation (boneProgram, "uSampler"), 0);

  return true;
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
  var model = mat4.create ();
  var shaderProgram;

  mat4.identity (DRAW_pMatrix);
  mat4.perspective (45, gl.viewportWidth / gl.viewportHeight, 0.1, 10000.0, DRAW_pMatrix);

  mat4.identity (DRAW_mvMatrix);
  mat4.rotate (DRAW_mvMatrix, Math.atan (up / distance), [1, 0, 0]);
  mat4.rotate (DRAW_mvMatrix, elapsed * 0.5 + Math.PI * 0.5, [0, 1, 0]);
  mat4.translate (DRAW_mvMatrix, [distance * Math.cos(elapsed * 0.5), -up, distance * Math.sin(elapsed * 0.5)]);
  mat4.translate (DRAW_mvMatrix, center);

  mat4.multiply (DRAW_mvMatrix, DRAW_modelMatrix, model); // Sets modelViewPersp to modelView * persp

  if (bindPose.length)
  {
    var i, pose = [];
    var poseArray;
    var frame, frameCount, offset;

    shaderProgram = boneProgram;
    gl.useProgram (shaderProgram);

    gl.bindBuffer (gl.ARRAY_BUFFER, vertexPositionBuffer);
    gl.vertexAttribPointer (shaderProgram.vertexPositionAttribute, 3, gl.FLOAT, false, 13 * 4, 0);
    gl.vertexAttribPointer (shaderProgram.textureCoordAttribute, 2, gl.FLOAT, false, 13 * 4, 3 * 4);
    gl.vertexAttribPointer (shaderProgram.boneWeightAttribute, 4, gl.FLOAT, false, 13 * 4, 5 * 4);
    gl.vertexAttribPointer (shaderProgram.boneIndexAttribute, 4, gl.FLOAT, false, 13 * 4, 9 * 4);

    frame = Math.floor (elapsed * 60.0);

    frameCount = takes[currentAnim].length / bindPose.length;
    frame %= frameCount;
    offset = frame * bindPose.length;

    for (i = 0; i < bindPose.length; ++i)
      {
        var relative;

        relative = mat4.create ();
        mat4.multiply (takes[currentAnim][offset + i], bindPose[i], relative);

        pose.push (relative[0]);
        pose.push (relative[1]);
        pose.push (relative[2]);
        pose.push (relative[12]);

        pose.push (relative[4]);
        pose.push (relative[5]);
        pose.push (relative[6]);
        pose.push (relative[13]);

        pose.push (relative[8]);
        pose.push (relative[9]);
        pose.push (relative[10]);
        pose.push (relative[14]);
      }

    poseArray = new Float32Array (pose);

    gl.uniform4fv (shaderProgram.bonesUniform, poseArray);
  }
  else
  {
    shaderProgram = staticProgram;
    gl.useProgram (shaderProgram);

    gl.bindBuffer (gl.ARRAY_BUFFER, vertexPositionBuffer);
    gl.vertexAttribPointer (shaderProgram.vertexPositionAttribute, 3, gl.FLOAT, false, 5 * 4, 0);
    gl.vertexAttribPointer (shaderProgram.textureCoordAttribute, 2, gl.FLOAT, false, 5 * 4, 3 * 4);
  }


  gl.uniformMatrix4fv (shaderProgram.pMatrixUniform, false, DRAW_pMatrix);
  gl.uniformMatrix4fv (shaderProgram.mvMatrixUniform, false, model);

  gl.activeTexture (gl.TEXTURE0);
  gl.bindTexture (gl.TEXTURE_2D, DRAW_currentTexture);

  gl.bindBuffer (gl.ELEMENT_ARRAY_BUFFER, vertexIndexBuffer);

  gl.drawElements (gl.TRIANGLES, indexCount, gl.UNSIGNED_SHORT, 0);
}


function DRAW_LoadModel (data)
{
  var meshes, inputTakes;

  meshes = data["meshes"];
  inputTakes = data["takes"];

  for (var i = 0; i < meshes.length; ++i)
    {
      var inputBindPose;

      if (meshes[i]["texture-URI"])
        DRAW_currentTexture = DRAW_LoadTexture (meshes[i]["texture-URI"]);

      DRAW_modelMatrix.set(meshes[i]["matrix"]);

      vertexPositionBuffer = gl.createBuffer ();
      gl.bindBuffer (gl.ARRAY_BUFFER, vertexPositionBuffer);
      gl.bufferData (gl.ARRAY_BUFFER, new Float32Array (meshes[i].vertices), gl.STATIC_DRAW);

      vertexIndexBuffer = gl.createBuffer ();
      gl.bindBuffer (gl.ELEMENT_ARRAY_BUFFER, vertexIndexBuffer);
      gl.bufferData (gl.ELEMENT_ARRAY_BUFFER, new Int16Array (meshes[i].triangles), gl.STATIC_DRAW);

      indexCount = meshes[i].triangles.length;

      inputBindPose = new Float32Array (meshes[i]["bind-pose"]);

      for (var j = 0; j + 15 < inputBindPose.length; j += 16)
        bindPose.push (inputBindPose.subarray (j, j + 16));

      minBounds = meshes[i]["min-bounds"];
      maxBounds = meshes[i]["max-bounds"];

      mat4.multiplyVec3 (DRAW_modelMatrix, minBounds);
      mat4.multiplyVec3 (DRAW_modelMatrix, maxBounds);

      center = [ -(minBounds[0] + maxBounds[0]) * 0.5,
                 -(minBounds[1] + maxBounds[1]) * 0.5,
                 -(minBounds[2] + maxBounds[2]) * 0.5 ];

      break; /* We support only one mesh for now */
    }

  if (inputTakes.length > 0)
    {
      for (var i = 0; i < inputTakes.length; ++i)
        {
          var option, take;
          var inputFrames;

          option = document.createElement("option");
          option.value = i;
          option.text = inputTakes[i].name;

          document.getElementById('take').appendChild(option);

          take = [];

          inputFrames = new Float32Array (inputTakes[i].frames);

          for (var k = 0; k + 15 < inputTakes[i].frames.length; k += 16)
            take.push(inputFrames.subarray(k, k + 16));

          takes.push(take);
        }

      document.getElementById('take').onchange = function (event) { currentAnim = document.getElementById('take').value; };
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

  if (!DRAW_SetupShaders ())
    return false;

  gl.clearColor (0.1, 0.1, 0.1, 1.0);
  gl.enable (gl.DEPTH_TEST);

  gl.enable (gl.BLEND);
  gl.blendFunc (gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA)

  return true;
}

/***********************************************************************/

var lastTime = 0;
var elapsed = 0;

function VIEWER_Init (modelURI)
{
  var httpreq = new XMLHttpRequest();

  if (!SYS_Init ())
    return;

  httpreq.open('GET', modelURI, true);

  httpreq.onreadystatechange =
    function ()
    {
      if (httpreq.readyState == 4)
        {
          DRAW_LoadModel (eval('(' + httpreq.responseText + ')'));

          VIEWER_Update ();
        }
    }

  httpreq.send(null);
}

function VIEWER_Update ()
{
  var timeNow, deltaTime;

  timeNow = new Date ().getTime ();
  deltaTime = (timeNow - lastTime) * 0.001;
  lastTime = timeNow;

  if (deltaTime < 0 || deltaTime > 0.05)
    deltaTime = 0.05;

  gl.viewport (0, 0, gl.viewportWidth, gl.viewportHeight);
  gl.clear (gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

  if (SYS_keys['Q'])
    {
      distance /= Math.pow (1.05, 1.0 + deltaTime);
      up /= Math.pow (1.05, 1.0 + deltaTime);
    }

  if (SYS_keys['A'])
    {
      distance *= Math.pow (1.05, 1.0 + deltaTime);
      up *= Math.pow (1.05, 1.0 + deltaTime);
    }

  if (SYS_keys['W'])
    up *= Math.pow (1.05, 1.0 + deltaTime);

  if (SYS_keys['S'])
    up /= Math.pow (1.05, 1.0 + deltaTime);

  DRAW_Flush ();

  elapsed += deltaTime;

  requestAnimFrame (VIEWER_Update);
}
