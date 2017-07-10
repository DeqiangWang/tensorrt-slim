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
#ifndef TFRT_NETWORK_H
#define TFRT_NETWORK_H

// #include <gflags/gflags.h>
// #include <glog/logging.h>

// #include <cmath>
#include <memory>
#include <string>
#include <sstream>

// // #include <cuda_runtime_api.h>
#include <NvInfer.h>

#include "tfrt_jetson.h"
#include "network.pb.h"

namespace tfrt
{
class scope;

/** Generic network class, implementation the basic building, inference
 * profiling methods.
 */
class network
{
public:
    /** Create network, specifying the name and the datatype.
     */
    network(std::string name, nvinfer1::DataType datatype) :
        m_pb_network(std::make_unique<tfrt_pb::network>()),
        m_nv_infer{nullptr}, m_nv_engine{nullptr}, m_nv_context{nullptr},
        m_max_batch_size{2}, m_workspace_size{16 << 20} {
    }
    ~network();
    /** Clear the network and its weights. */
    void clear();

    /** Load network configuration and weights + build + profile model. */
    bool load(std::string filename);
    /** Get the default scope for this network. */
    tfrt::scope scope(nvinfer1::INetworkDefinition* nv_network) const;

public:
    /** Get a tensor by name. Return empty tensor if not found.
     */
    const tfrt_pb::tensor& tensor_by_name(std::string name) const;
    /** Get NV weights by name. Return empty weights if not found.
     */
    nvinfer1::Weights weights_by_name(std::string name) const;

    const std::string& name() const;
    network& name(const std::string& name);
    nvinfer1::DataType datatype() const;

    nvinfer1::DimsCHW input_shape() const;
    const std::string& input_name() const;
    std::vector<nvinfer1::DimsCHW> outputs_shape() const;
    std::vector<std::string> outputs_name() const;


public:
    /** Generate empty weights. */
    nvinfer1::Weights empty_weights() const {
        return nvinfer1::Weights{.type = this->datatype(), .values = nullptr, .count = 0};
    }
    /** Convert TF protobuf tensor to NV weights. */
    static nvinfer1::Weights tensor_to_weights(const tfrt_pb::tensor& tensor);

protected:
    // Protected setters...
    // void name(std::string name);
    /** Load weights and configuration from .tfrt file. */
    bool load_weights(const std::string& filename);
    /** Clear out the collections of network weights, to save memory. */
    void clear_weights();

    /** Build the complete network.
     * To be re-defined in children classes.
     */
    virtual nvinfer1::ITensor* build(tfrt::scope sc);
    /** Serialize a network model. If caching=True, try to first load from
     * a cached file. If no file, construct the usual way and save the cache.
     */
    bool serialize_model(const std::string& filename, std::stringstream& model_stream, bool caching=true);
    /** Build and profile a model
     */
    bool profile_model(std::stringstream& model_stream);

protected:
	/** Prefix used for tagging printed log output. */
	#define LOG_GIE "[GIE]  "

	/** Logger class for GIE info/warning/errors.
     */
	class Logger : public nvinfer1::ILogger
	{
		void log( Severity severity, const char* msg ) override
		{
			if( severity != Severity::kINFO /*|| mEnableDebug*/ )
				printf(LOG_GIE "%s\n", msg);
		}
	} m_gie_logger;
	/** Profiler interface for measuring layer timings
	 */
	class Profiler : public nvinfer1::IProfiler
	{
	public:
		Profiler() : timingAccumulator(0.0f) {}
		virtual void reportLayerTime(const char* layerName, float ms)
		{
			printf(LOG_GIE "layer %s - %f ms\n", layerName, ms);
			timingAccumulator += ms;
		}
		float timingAccumulator;

	} m_gie_profiler;
	/** When profiling is enabled, end a profiling section and report timing statistics.
	 */
	inline void PROFILER_REPORT()	{
        if(mEnableProfiler) {
            printf(LOG_GIE "layer network time - %f ms\n", m_gie_profiler.timingAccumulator); m_gie_profiler.timingAccumulator = 0.0f;
        }
    }

protected:
    // Protobuf network object.
    std::unique_ptr<tfrt_pb::network>  m_pb_network;
    // TensorRT elements...
    nvinfer1::IRuntime*  m_nv_infer;
	nvinfer1::ICudaEngine*  m_nv_engine;
	nvinfer1::IExecutionContext*  m_nv_context;

    uint32_t m_max_batch_size;
    uint32_t m_workspace_size;

	// uint32_t mWidth;
	// uint32_t mHeight;
	// uint32_t mInputSize;
	// float*   mInputCPU;
	// float*   mInputCUDA;
	// uint32_t mMaxBatchSize;
	bool	 mEnableProfiler;
	bool     mEnableDebug;
	// bool	 mEnableFP16;
	// bool     mOverride16;

    // Temporary collection of zero tensors.
    std::vector<tfrt_pb::tensor>  m_zero_tensors;
};

}

#endif