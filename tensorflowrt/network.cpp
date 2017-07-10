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
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <fstream>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

#include "scope.h"
#include "network.h"
#include "tensorflowrt.h"

const int kProtoReadBytesLimit = INT_MAX;

namespace tfrt {

using google::protobuf::io::FileInputStream;
using google::protobuf::io::FileOutputStream;
using google::protobuf::io::ZeroCopyInputStream;
using google::protobuf::io::CodedInputStream;
using google::protobuf::io::ZeroCopyOutputStream;
using google::protobuf::io::CodedOutputStream;
// using google::protobuf::Message;

/* ============================================================================
 * Nice utils
 * ========================================================================== */
inline nvinfer1::Dims tensor_shape(const tfrt_pb::tensor& t)
{
    nvinfer1::Dims dims;
    for(int i = 0; i < t.shape_size(); i++) {
        dims.d[i] = t.shape(i);
    }
    dims.nbDims = t.shape_size();
    return dims;
}
// /** Find tensor in GIE network. */
// nvinfer1::ILayer* find_layer(nvinfer1::INetworkDefinition* network)
// {
//     nvinfer1::ILayer* layer{nullptr};
//     return layer;
// }
// /** Mark layer outputs. */
// void mark_layer_outputs(nvinfer1::ILayer* layer, nvinfer1::INetworkDefinition* network)
// {
//     for(int i = 0 ; i < layer->getNbOutputs() ; ++i) {
//         network->markOutput(*layer->getOutput(i));
//     }
// }

/* ============================================================================
 * tfrt::network methods.
 * ========================================================================== */
network::~network()
{
    // TODO: unique_ptr + custom deleter.
    if(m_nv_engine) {
		m_nv_engine->destroy();
		m_nv_engine = nullptr;
	}
	if(m_nv_infer) {
		m_nv_infer->destroy();
		m_nv_infer = nullptr;
	}
}
void network::clear()
{
    m_pb_network->Clear();
    m_zero_tensors.clear();
}
tfrt::scope network::scope(nvinfer1::INetworkDefinition* nv_network) const
{
    return tfrt::scope(nv_network, this, this->name());
}

bool network::load(std::string filename)
{
    // Start by loading configuration + weights.
    this->load_weights(filename);

    return true;
}


/* ============================================================================
 * Getters / setters wrapping protobuf methods.
 * ========================================================================== */
const tfrt_pb::tensor& network::tensor_by_name(std::string name) const
{
    // Best search algorithm ever!
    for(int i = 0 ; i < m_pb_network->weights_size() ; ++i) {
        const tfrt_pb::tensor& tensor = m_pb_network->weights(i);
        if(tensor.name() == name) {
            LOG(INFO) << "FOUND tfrt_pb::tensor '" << name << "'. "
                << "SHAPE: " << dims_str(tensor_shape(tensor)) << " "
                << "SIZE: " << tensor.size();
            return tensor;
        }
    }
    // Default empty tensor.
    LOG(WARNING) << "FAILED to find the tfrt_pb::tensor '" << name
        << "'. Using default empty tensor." ;
    return tfrt_pb::tensor::default_instance();
}
nvinfer1::Weights network::weights_by_name(std::string name) const
{
    const tfrt_pb::tensor& tensor = tensor_by_name(name);
    return tensor_to_weights(tensor);
}

nvinfer1::Weights network::tensor_to_weights(const tfrt_pb::tensor& tensor)
{
    nvinfer1::Weights w{
        .type = nvinfer1::DataType(int(tensor.datatype())),
        .values = tensor.data().data(),
        .count = int(tensor.size())
    };
    if(w.count == 0) {
        w.values = nullptr;
    }
    return w;
}
const std::string& network::name() const {
    return m_pb_network->name();
}
network& network::name(const std::string& name) {
    m_pb_network->set_name(name);
    return *this;
}
nvinfer1::DataType network::datatype() const {
    // Datatype of the network. Hopefully consistent with weights...
    auto dt = m_pb_network->datatype();
    return nvinfer1::DataType(int(dt));
}

nvinfer1::DimsCHW network::input_shape() const
{
    auto input = m_pb_network->input();
    return nvinfer1::DimsCHW{input.c(), input.h(), input.w()};
}
const std::string& network::input_name() const
{
    return m_pb_network->input().name();
}
std::vector<nvinfer1::DimsCHW> network::outputs_shape() const
{
    std::vector<nvinfer1::DimsCHW> v;
    for(int i = 0 ; i < m_pb_network->outputs_size() ; ++i) {
        const tfrt_pb::output& output = m_pb_network->outputs(i);
        v.push_back(nvinfer1::DimsCHW{output.c(), output.h(), output.w()});
    }
    return v;
}
std::vector<std::string> network::outputs_name() const
{
    std::vector<std::string> v;
    for(int i = 0 ; i < m_pb_network->outputs_size() ; ++i) {
        const tfrt_pb::output& output = m_pb_network->outputs(i);
        v.push_back(output.name());
    }
    return v;
}

/* ============================================================================
 * Private tfrt::network methods... Tambouille interne.
 * ========================================================================== */
bool network::load_weights(const std::string& filename)
{
    LOG(INFO) << "Loading network parameters and weights from: " << filename;
    // Highly inspired by Caffe source code!
    int fd = open(filename.c_str(), O_RDONLY);
    CHECK_NE(fd, -1) << "FILE not found: " << filename;
    ZeroCopyInputStream* raw_input = new FileInputStream(fd);
    CodedInputStream* coded_input = new CodedInputStream(raw_input);
    coded_input->SetTotalBytesLimit(kProtoReadBytesLimit, 536870912);
    bool success = m_pb_network->ParseFromCodedStream(coded_input);
    delete coded_input;
    delete raw_input;
    close(fd);
    CHECK(success) << "FATAL error, could not parse network from protobuf file: " << filename;
    return success;
}
void network::clear_weights()
{
    m_pb_network->clear_weights();
}

nvinfer1::ITensor* network::build(tfrt::scope sc)
{
    return nullptr;
}
bool network::serialize_model(const std::string& filename, std::stringstream& model_stream, bool caching)
{
    // Load model parameters and weights.
    this->load_weights(filename);

    // Set model stream back to beginning.
    model_stream.seekg(0, model_stream.beg);
    // Try to read serialized model from cache.
    std::ostringstream  filename_cache;
    filename_cache << filename << "."  << m_max_batch_size << ".cache";
    if(caching) {
        LOG(INFO) << LOG_GIE << "Try reading cached model from: "<< filename_cache.str();
        // Successful read of cached file => load and return.
        std::ifstream model_cached(filename_cache.str());
        if(model_cached) {
            LOG(INFO) << LOG_GIE << "Loading network profile from cache...";
            model_stream << model_cached.rdbuf();
            model_cached.close();
            return true;
        }
        LOG(WARNING) << LOG_GIE << "Could not read cached model. Back to th' old way.";
    }
    LOG(INFO) << LOG_GIE << "Building and profiling the network model.";
    this->profile_model(model_stream);
    this->clear_weights();

    if(caching) {
        LOG(INFO) << LOG_GIE << "Writing cached model to: " << filename_cache.str();
        std::ofstream model_cached;
		model_cached.open(filename_cache.str());
		model_cached << model_stream.rdbuf();
		model_cached.close();
		model_stream.seekg(0, model_stream.beg);
    }
    return true;
}
bool network::profile_model(std::stringstream& model_stream)
{
    // Create API root class - must span the lifetime of the engine usage.
	nvinfer1::IBuilder* builder = nvinfer1::createInferBuilder(m_gie_logger);
	nvinfer1::INetworkDefinition* network = builder->createNetwork();

	builder->setDebugSync(mEnableDebug);
	builder->setMinFindIterations(10);	    // allow time for TX1/2 GPU to spin up.
    builder->setAverageFindIterations(5);

    // Build the network.
    LOG(INFO) << LOG_GIE << "Building network from scratch!";
    auto net = this->build(this->scope(network));
    CHECK_NOTNULL(net);
    LOG(INFO) << LOG_GIE << "Network successfully built."
        << " #Inputs: " << network->getNbInputs() << " #Outputs: " << network->getNbOutputs();

	// Build the engine
    LOG(INFO) << LOG_GIE << "Configuring CUDA engine.";
	builder->setMaxBatchSize(m_max_batch_size);
	builder->setMaxWorkspaceSize(m_workspace_size);
	// set up the network for paired-fp16 format
	if(this->datatype() == nvinfer1::DataType::kHALF) {
		builder->setHalf2Mode(true);
    }
    nvinfer1::ICudaEngine* engine = builder->buildCudaEngine(*network);
    CHECK_NOTNULL(engine);

	network->destroy();
	// Serialize the engine, then close everything down
	LOG(INFO) << LOG_GIE << "Serializing the engine.";
    engine->serialize(model_stream);
	engine->destroy();
	builder->destroy();
    return true;
}

}
