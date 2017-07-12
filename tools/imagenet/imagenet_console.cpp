/* ============================================================================
# [2017] - Robik AI Ltd - Paul Balanca
# All Rights Reserved.

# NOTICE: All information contained herein is, and remains
# the property of Robik AI Ltd, and its suppliers
# if any.  The intellectual and technical concepts contained
# herein are proprietary to Robik AI Ltd
# and its suppliers and may be covered by U.S., European and Foreign Patents,
# patents in process, and are protected by trade secret or copyright law.
# Dissemination of this information or reproduction of this material
# is strictly forbidden unless prior written permission is obtained
# from Robik AI Ltd.
# =========================================================================== */
#include <glog/logging.h>
#include <gflags/gflags.h>

#include <map>
#include <string>

// TensorFlowRT headers
#include <tensorflowrt.h>
#include <tensorflowrt_util.h>

// FLAGS...
DEFINE_string(network, "inception1", "ImageNet network to test.");
DEFINE_string(network_pb, "inception1", "Network protobuf parameter file.");
DEFINE_string(image, "../data/", "Image to classify.");
DEFINE_string(imagenet_info, "../data/", "ImageNet information (classes, ...).");
// DEFINE_bool(verbose, false, "Display program name before message");
// DEFINE_string(message, "Hello world!", "Message to print");
// DEFINE_int32(input_height, 224, "Network input height.");

/** Global map containing the list of available ImageNet networks.
 */
std::map<std::string, tfrt::imagenet_network*> NetworksMap = {
    {"inception1", nullptr},
    {"inception2", nullptr}
};


int main( int argc, char** argv )
{
    LOG(INFO) << "<<< ImageNet Console | Network " << FLAGS_network << " >>>";
    // Get network and load parameters & weights.
    tfrt::imagenet_network* network = NetworksMap.at(FLAGS_network);
    network->load(FLAGS_network_pb);
    network->load_info(FLAGS_imagenet_info);


    // printf("imagenet-console\n  args (%i):  ", argc);

    // for( int i=0; i < argc; i++ )
    // 	printf("%i [%s]  ", i, argv[i]);

    // printf("\n\n");


    // // retrieve filename argument
    // if( argc < 2 )
    // {
    // 	printf("imagenet-console:   input image filename required\n");
    // 	return 0;
    // }

    // const char* imgFilename = argv[1];


    // // create imageNet
    // imageNet* net = imageNet::Create(argc, argv);

    // if( !net )
    // {
    // 	printf("imagenet-console:   failed to initialize imageNet\n");
    // 	return 0;
    // }

    // net->EnableProfiler();

    // // load image from file on disk
    // float* imgCPU    = NULL;
    // float* imgCUDA   = NULL;
    // int    imgWidth  = 0;
    // int    imgHeight = 0;

    // if( !loadImageRGBA(imgFilename, (float4**)&imgCPU, (float4**)&imgCUDA, &imgWidth, &imgHeight) )
    // {
    // 	printf("failed to load image '%s'\n", imgFilename);
    // 	return 0;
    // }

    // float confidence = 0.0f;

    // // classify image
    // const int img_class = net->Classify(imgCUDA, imgWidth, imgHeight, &confidence);

    // if( img_class >= 0 )
    // {
    // 	printf("imagenet-console:  '%s' -> %2.5f%% class #%i (%s)\n", imgFilename, confidence * 100.0f, img_class, net->GetClassDesc(img_class));

    // 	if( argc > 2 )
    // 	{
    // 		const char* outputFilename = argv[2];

    // 		// overlay the classification on the image
    // 		cudaFont* font = cudaFont::Create();

    // 		if( font != NULL )
    // 		{
    // 			char str[512];
    // 			sprintf(str, "%2.3f%% %s", confidence * 100.0f, net->GetClassDesc(img_class));

    // 			const int overlay_x = 10;
    // 			const int overlay_y = 10;
    // 			const int px_offset = overlay_y * imgWidth * 4 + overlay_x * 4;

    // 			// if the image has a white background, use black text (otherwise, white)
    // 			const float white_cutoff = 225.0f;
    // 			bool white_background = false;

    // 			if( imgCPU[px_offset] > white_cutoff && imgCPU[px_offset + 1] > white_cutoff && imgCPU[px_offset + 2] > white_cutoff )
    // 				white_background = true;

    // 			// overlay the text on the image
    // 			font->RenderOverlay((float4*)imgCUDA, (float4*)imgCUDA, imgWidth, imgHeight, (const char*)str, 10, 10,
    // 							white_background ? make_float4(0.0f, 0.0f, 0.0f, 255.0f) : make_float4(255.0f, 255.0f, 255.0f, 255.0f));
    // 		}

    // 		printf("imagenet-console:  attempting to save output image to '%s'\n", outputFilename);

    // 		if( !saveImageRGBA(outputFilename, (float4*)imgCPU, imgWidth, imgHeight) )
    // 			printf("imagenet-console:  failed to save output image to '%s'\n", outputFilename);
    // 		else
    // 			printf("imagenet-console:  completed saving '%s'\n", outputFilename);
    // 	}
    // }
    // else
    // 	printf("imagenet-console:  failed to classify '%s'  (result=%i)\n", imgFilename, img_class);

    // printf("\nshutting down...\n");
    // CUDA(cudaFreeHost(imgCPU));
    // delete net;
    // return 0;
}