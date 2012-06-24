#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

#define FBXSDK_NEW_API

#include <fbxsdk.h>
#include <fbxsdk/utils/fbxutilities.h>

static int FbxConvert_printHelp;
static int FbxConvert_printVersion;
static const char *FbxConvert_format = "binary";

static struct option FbxConvert_longOptions[] =
{
    { "format", required_argument, 0, 'f' },
    { "help",     no_argument, &FbxConvert_printHelp, 1 },
    { "version",  no_argument, &FbxConvert_printVersion, 1 },
    { 0, 0, 0, 0 }
};

static void
PreparePointCacheData(FbxScene* pScene);

static void
ConvertNurbsAndPatchRecursive(FbxManager* pSdkManager,
                              FbxNode* node);

void
ExportMeshBinaryRecursive (FbxNode* node,
                           FbxAMatrix& pParentGlobalPosition);

void
ExportTakeBinary (FbxScene *scene, FbxString *takeName);

void
FbxConvert_ExportMeshAsJSON (FbxNode* node,
                             FbxAMatrix& pParentGlobalPosition,
                             int *first);

int
main(int argc, char** argv)
{
  int i;
  int lFileFormat = -1;
  FbxString *gFileName = new FbxString;

  FbxManager* FbxManager;
  FbxImporter* fbxImporter;
  FbxScene* scene;

  FbxArray<FbxString*> takeNames;

  while(-1 != (i = getopt_long(argc, argv, "", FbxConvert_longOptions, NULL)))
    {
      switch(i)
        {
        case 0:

          break;

        case 'f':

          FbxConvert_format = optarg;

          break;

        case '?':

          fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);

          return EXIT_FAILURE;
        }
    }

  if(FbxConvert_printHelp)
    {
      fprintf(stdout,
              "Usage: %s [OPTION]... FILENAME\n"
              "\n"
              "      --help     display this help and exit\n"
              "      --version  display version information\n"
              "\n"
              "Report bugs to <morten.hustveit@gmail.com>\n", argv[0]);

      return EXIT_SUCCESS;
    }

  if (optind + 1 != argc)
    errx (EX_USAGE, "Usage: %s [OPTION]... FILENAME", argv[0]);

  *gFileName = argv[optind];

  // The first thing to do is to create the FBX SDK manager which is the
  // object allocator for almost all the classes in the SDK.
  if (!(FbxManager = FbxManager::Create()))
    errx (EXIT_FAILURE, "Unable to create the FBX SDK manager");

  // Load plugins from the executable directory
  FbxString lPath = FbxGetApplicationDirectory();
  FbxString lExtension = "so";

  FbxManager->LoadPluginsDirectory(lPath.Buffer(), lExtension.Buffer());

  // Create the entity that will hold the scene.
  scene = FbxScene::Create(FbxManager, "");

  if (!FbxManager)
    fprintf (stderr, "Unable to create the FBX SDK manager\n");

  fbxImporter = FbxImporter::Create(FbxManager, "");

  if (!FbxManager->GetIOPluginRegistry()->DetectReaderFileFormat(*gFileName, lFileFormat))
    lFileFormat = FbxManager->GetIOPluginRegistry()->FindReaderIDByDescription ("FBX binary (*.fbx)");;

  if(!fbxImporter->Initialize(gFileName->Buffer(), lFileFormat))
    errx (EXIT_FAILURE, "Unable to open `%s': %s", (const char *) *gFileName, (const char *) fbxImporter->GetLastErrorString ());

  if(!fbxImporter->Import(scene))
    errx (EXIT_FAILURE, "Unable to import `%s': %s", (const char *) *gFileName, (const char *) fbxImporter->GetLastErrorString ());

  // Convert Axis System to what is used in this example, if needed
  FbxAxisSystem OurAxisSystem (FbxAxisSystem::eYAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eRightHanded);

  if (scene->GetGlobalSettings().GetSystemUnit().GetScaleFactor() != 100.0)
    FbxSystemUnit (100.0).ConvertScene (scene);

  if (scene->GetGlobalSettings().GetAxisSystem() != OurAxisSystem)
    OurAxisSystem.ConvertScene(scene);

  // Nurbs and patch attribute types are not supported yet.
  // Convert them into mesh node attributes to have them drawn.
  ConvertNurbsAndPatchRecursive(FbxManager, scene->GetRootNode());

  // Convert any .PC2 point cache data into the .MC format for
  // vertex cache deformer playback.
  PreparePointCacheData(scene);

  scene->FillAnimStackNameArray (takeNames);

  fbxImporter->Destroy();

  if (!strcmp (FbxConvert_format, "binary"))
    {
      FbxAMatrix lDummyGlobalPosition;

      int i, lCount = scene->GetRootNode()->GetChildCount();

      for (i = 0; i < lCount; i++)
        ExportMeshBinaryRecursive(scene->GetRootNode()->GetChild(i), lDummyGlobalPosition);

      for (i = 0; i < takeNames.GetCount(); i++)
        ExportTakeBinary (scene, takeNames[i]);
    }
  else if (!strcmp (FbxConvert_format, "json"))
    {
      FbxAMatrix lDummyGlobalPosition;
      int first = 1;

      int i, lCount = scene->GetRootNode()->GetChildCount();

      printf ("[");

      for (i = 0; i < lCount; i++)
        FbxConvert_ExportMeshAsJSON (scene->GetRootNode()->GetChild(i), lDummyGlobalPosition, &first);
      printf ("]");
    }
  else
    {
      fprintf (stderr, "Unknown format: %s\n", FbxConvert_format);

      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}

static void
ConvertNurbsAndPatchRecursive(FbxManager* pSdkManager, FbxNode* node)
{
  FbxNodeAttribute* lNodeAttribute;
  int i, count;

  lNodeAttribute = node->GetNodeAttribute();

  if (lNodeAttribute
      && (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eNurbs
          || lNodeAttribute->GetAttributeType() == FbxNodeAttribute::ePatch))
    {
      FbxGeometryConverter lConverter(pSdkManager);

      lConverter.TriangulateInPlace (node);
    }

  count = node->GetChildCount();

  for (i = 0; i < count; ++i)
    ConvertNurbsAndPatchRecursive(pSdkManager, node->GetChild(i));
}

static void
PreparePointCacheData(FbxScene* pScene)
{
  // This function show how to cycle thru scene elements in a linear way.
  int lIndex, lNodeCount;

  lNodeCount = FbxGetSrcCount<FbxNode>(pScene);

  for (lIndex = 0; lIndex < lNodeCount; lIndex++)
    {
      FbxNode* lNode;

      lNode = FbxGetSrc<FbxNode>(pScene, lIndex);

      if (lNode->GetGeometry())
        {
          int i, lVertexCacheDeformerCount;

          lVertexCacheDeformerCount = lNode->GetGeometry()->GetDeformerCount(FbxDeformer::eVertexCache);

          // There should be a maximum of 1 Vertex Cache Deformer for the moment
          lVertexCacheDeformerCount = lVertexCacheDeformerCount > 0 ? 1 : 0;

          for (i = 0; i < lVertexCacheDeformerCount; ++i)
            {
              FbxVertexCacheDeformer* lDeformer;

              lDeformer = static_cast<FbxVertexCacheDeformer*>(lNode->GetGeometry()->GetDeformer(i, FbxDeformer::eVertexCache));

              if(!lDeformer)
                continue;

              FbxCache* lCache = lDeformer->GetCache();

              if(!lCache)
                continue;

              // Process the point cache data only if the constraint is active
              if (!lDeformer->IsActive())
                continue;

              if (lCache->GetCacheFileFormat() == FbxCache::eMayaCache)
                {
                  if (!lCache->ConvertFromMCToPC2(FbxTime::GetFrameRate(pScene->GetGlobalSettings().GetTimeMode()), 0))
                    {
                      // Conversion failed, retrieve the error here
                      FbxString lTheErrorIs = lCache->GetError().GetLastErrorString();
                    }
                }

              // Now open the cache file to read from it
              if (!lCache->OpenFileForRead())
                {
                  // Cannot open file
                  FbxString lTheErrorIs = lCache->GetError().GetLastErrorString();

                  // Set the deformer inactive so we don't play it back
                  lDeformer->SetActive(false);
                }
            }
        }
    }
}
