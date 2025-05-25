#include "NvInfer.h"
#include "NvInferRuntime.h"   // For runtime APIs
#include "NvOnnxParser.h"     // If parsing ONNX model (optional)
#include "cuda_runtime_api.h" // For CUDA memory operations
#include <memory>
using namespace nvinfer1;

class Logger : public ILogger
{
    void log(Severity severity, const char *msg) noexcept override
    {
        if (severity <= Severity::kINFO)
            std::cout << "[TensorRT] " << msg << std::endl;
    }
};

class TensorRTManager
{
public:
    int predictedDigit = -1;
    int getPrediction() const {return predictedDigit;}


    Logger logger;
    struct SampleParams
    {
        std::vector<std::string> inputTensorNames;
        std::vector<std::string> outputTensorNames;
    };

    struct OnnxSampleParams : public SampleParams
    {
        std::string onnxFileName; //!< Filename of ONNX file of a network
    };

    OnnxSampleParams mParams;
    nvinfer1::Dims mInputDims;                    //!< The dimensions of the input to the network.
    nvinfer1::Dims mOutputDims;                   //!< The dimensions of the output to the network.
    std::shared_ptr<nvinfer1::IRuntime> mRuntime; //!< The TensorRT runtime used to deserialize the engine
    std::shared_ptr<nvinfer1::ICudaEngine> mEngine;
    std::shared_ptr<nvinfer1::IExecutionContext> mContext;

    TensorRTManager(cudaStream_t &stream)
    {
        mParams.onnxFileName = "tensorModels/mnist.onnx";
        mParams.inputTensorNames.push_back("Input3");
        mParams.outputTensorNames.push_back("Plus214_Output_0");

        build();

        std::cout << "TensorRTManager is initialized...\n";
    }

    ~TensorRTManager()
    {
        // Cleanup if needed
    }
    bool build()
    {
        auto builder = std::shared_ptr<nvinfer1::IBuilder>(nvinfer1::createInferBuilder(logger));
        if (!builder)
        {
            return false;
        }

        auto network = std::shared_ptr<nvinfer1::INetworkDefinition>(builder->createNetworkV2(0));
        if (!network)
        {
            return false;
        }

        auto config = std::shared_ptr<nvinfer1::IBuilderConfig>(builder->createBuilderConfig());
        if (!config)
        {
            return false;
        }

        auto parser = std::shared_ptr<nvonnxparser::IParser>(nvonnxparser::createParser(*network, logger));
        if (!parser)
        {
            return false;
        }
        if (!parser->parseFromFile(mParams.onnxFileName.c_str(), static_cast<int>(nvinfer1::ILogger::Severity::kWARNING)))
        {
            std::cerr << "ERROR: could not parse ONNX model." << std::endl;
            return false;
        }
        auto timingCache = std::shared_ptr<nvinfer1::ITimingCache>();

        std::shared_ptr<IHostMemory> plan{builder->buildSerializedNetwork(*network, *config)};
        if (!plan)
        {
            return false;
        }

        mRuntime = std::shared_ptr<nvinfer1::IRuntime>(createInferRuntime(logger));
        if (!mRuntime)
        {
            return false;
        }

        mEngine = std::shared_ptr<nvinfer1::ICudaEngine>(
            mRuntime->deserializeCudaEngine(plan->data(), plan->size()));
        if (!mEngine)
        {
            return false;
        }

        mContext = std::shared_ptr<nvinfer1::IExecutionContext>(mEngine->createExecutionContext());
        if (!mContext)
        {
            std::cerr << "Failed to create execution context!" << std::endl;
            return false;
        }
        assert(network->getNbInputs() == 1);
        mInputDims = network->getInput(0)->getDimensions();
        assert(mInputDims.nbDims == 4);

        assert(network->getNbOutputs() == 1);
        mOutputDims = network->getOutput(0)->getDimensions();
        assert(mOutputDims.nbDims == 2);

        return true;
    }

    bool infer(float *d_input, cudaStream_t &stream)
    {
        float *d_output;
        cudaMalloc(&d_output, 10 * sizeof(float));

        // Print tensor names and modes
        for (int i = 0, e = mEngine->getNbIOTensors(); i < e; ++i)
        {
            const char *tensorName = mEngine->getIOTensorName(i);
            nvinfer1::TensorIOMode mode = mEngine->getTensorIOMode(tensorName);
            if (mode == nvinfer1::TensorIOMode::kINPUT)
                mContext->setTensorAddress("Input3", d_input);
            else
                mContext->setTensorAddress("Plus214_Output_0", d_output);
        }

        if (!mContext->enqueueV3(stream))
        {
            std::cerr << "Failed to run inference!" << std::endl;
            return false;
        }

        float h_output[10];

        cudaMemcpyAsync(h_output, d_output, 10 * sizeof(float), cudaMemcpyDeviceToHost, stream);
        cudaStreamSynchronize(stream);

        int maxIdx = 0;
        for (int i = 1; i < 10; ++i)
            if (h_output[i] > h_output[maxIdx])
                maxIdx = i;
        
        predictedDigit = maxIdx; 
        cudaFree(d_output);
        return true;
    }
};
