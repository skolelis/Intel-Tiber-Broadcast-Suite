
#include "ffmpeg_pipeline_generator.hpp"
#include "config_serialize_deserialize.hpp"
#include <sstream>
#include "CmdPassImpl.h"

// Helper function to convert string to any type
template <typename T>
T from_string(const std::string& str) {
    std::istringstream iss(str);
    T value;
    iss >> value;
    return value;
}

// Function to convert vector of string pairs to FrameRate
static FrameRate stringPairsToFrameRate(const std::unordered_map<std::string, std::string>& pairs, const std::string& prefix) {
    FrameRate frameRate;
    frameRate.numerator = from_string<int>(pairs.at(prefix + "frame_rate_numerator"));
    frameRate.denominator = from_string<int>(pairs.at(prefix + "frame_rate_denominator"));
    return frameRate;
}

// Function to convert vector of string pairs to Video
static Video stringPairsToVideo(const std::unordered_map<std::string, std::string>& pairs, const std::string& prefix) {
    Video video;
    video.frame_width = from_string<int>(pairs.at(prefix + "frame_width"));
    video.frame_height = from_string<int>(pairs.at(prefix + "frame_height"));
    video.pixel_format = pairs.at(prefix + "pixel_format");
    video.video_type = pairs.at(prefix + "video_type");
    video.frame_rate = stringPairsToFrameRate(pairs, prefix);
    return video;
}

// Function to convert vector of string pairs to Audio
static Audio stringPairsToAudio(const std::unordered_map<std::string, std::string>& pairs, const std::string& prefix) {
    Audio audio;
    audio.channels = from_string<int>(pairs.at(prefix + "channels"));
    audio.sample_rate = from_string<int>(pairs.at(prefix + "sample_rate"));
    audio.format = pairs.at(prefix + "format");
    audio.packet_time = pairs.at(prefix + "packet_time");
    return audio;
}

// Function to convert vector of string pairs to File
static File stringPairsToFile(const std::unordered_map<std::string, std::string>& pairs, const std::string& prefix) {
    File file;
    file.path = pairs.at(prefix + "file_path");
    file.filename = pairs.at(prefix + "file_filename");
    return file;
}

// Function to convert vector of string pairs to ST2110
static ST2110 stringPairsToST2110(const std::unordered_map<std::string, std::string>& pairs, const std::string& prefix) {
    ST2110 st2110;
    st2110.network_interface = pairs.at(prefix + "network_interface");
    st2110.local_ip = pairs.at(prefix + "local_ip");
    st2110.remote_ip = pairs.at(prefix + "remote_ip");
    st2110.transport = pairs.at(prefix + "transport");
    st2110.remote_port = from_string<int>(pairs.at(prefix + "remote_port"));
    st2110.payload_type = from_string<int>(pairs.at(prefix + "st_payload_type"));
    return st2110;
}

// Function to convert vector of string pairs to MCM
static MCM stringPairsToMCM(const std::unordered_map<std::string, std::string>& pairs, const std::string& prefix) {
    MCM mcm;
    mcm.conn_type = pairs.at(prefix + "conn_type");
    mcm.transport = pairs.at(prefix + "transport");
    mcm.transport_pixel_format = pairs.at(prefix + "transport_pixel_format");
    mcm.ip = pairs.at(prefix + "ip");
    mcm.port = from_string<int>(pairs.at(prefix + "port"));
    mcm.urn = pairs.at(prefix + "urn");
    return mcm;
}

// Function to convert vector of string pairs to Payload
static Payload stringPairsToPayload(const std::unordered_map<std::string, std::string>& pairs, const std::string& prefix) {
    Payload payload;
    payload.type = static_cast<payload_type>(from_string<int>(pairs.at(prefix + "payload_type")));
    if (payload.type == video) {
        payload.video = stringPairsToVideo(pairs, prefix);
    } else if (payload.type == audio) {
        payload.audio = stringPairsToAudio(pairs, prefix);
    }
    return payload;
}

// Function to convert vector of string pairs to StreamType
static StreamType stringPairsToStreamType(const std::unordered_map<std::string, std::string>& pairs, const std::string& prefix) {
    StreamType streamType;
    streamType.type = static_cast<stream_type>(from_string<int>(pairs.at(prefix + "stream_type")));
    if (streamType.type == file) {
        streamType.file = stringPairsToFile(pairs, prefix);
    } else if (streamType.type == st2110) {
        streamType.st2110 = stringPairsToST2110(pairs, prefix);
    } else if (streamType.type == mcm) {
        streamType.mcm = stringPairsToMCM(pairs, prefix);
    }
    return streamType;
}

// Function to convert vector of string pairs to Stream
static Stream stringPairsToStream(const std::unordered_map<std::string, std::string>& pairs, const std::string& prefix) {
    Stream stream;
    stream.payload = stringPairsToPayload(pairs, prefix);
    stream.stream_type = stringPairsToStreamType(pairs, prefix);
    return stream;
}

// Function to convert vector of string pairs to Config
static Config stringPairsToConfig(const std::vector<std::pair<std::string, std::string>>& pairs) {
    Config config;

    if (pairs.front().first == "json") {
        if (deserialize_config_json(config, pairs.front().second) != 0) {
            std::cout << "Error deserializing Config from json, trying previous solution" << std::endl;
        }
        else {
            return config;
        }
    }

    std::unordered_map<std::string, std::string> pairs_map(pairs.begin(), pairs.end());
    config.function = pairs_map.at("function");
    config.gpu_hw_acceleration = pairs_map.at("gpu_hw_acceleration");
    config.logging_level = from_string<int>(pairs_map.at("logging_level"));

    // Extract senders and receivers
    size_t sender_index = 0;
    size_t receiver_index = 0;
    while (true) {
        std::string sender_prefix = "sender_" + std::to_string(sender_index) + "_";
        std::string receiver_prefix = "receiver_" + std::to_string(receiver_index) + "_";
        if (pairs_map.find(sender_prefix + "payload_type") != pairs_map.end()) {
            config.senders.push_back(stringPairsToStream(pairs_map, sender_prefix));
            ++sender_index;
        } else if (pairs_map.find(receiver_prefix + "payload_type") != pairs_map.end()) {
            config.receivers.push_back(stringPairsToStream(pairs_map, receiver_prefix));
            ++receiver_index;
        } else {
            break;
        }
    }
    return config;
}

void CmdPassImpl::Run(std::string server_address) {
    ServerBuilder builder;
    stop = false;

    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);

    cq_ = builder.AddCompletionQueue();

    server_ = builder.BuildAndStart();

    std::cout << "[*] Server run and listening on " << server_address << std::endl;

    std::jthread th([&]() {
        stop.wait(false);
        std::cout << "[*] Shutting down the server" << std::endl;

        server_->Shutdown();
        cq_->Shutdown();

        void *ignored_tag;
        bool ignored_ok;
        while (cq_->Next(&ignored_tag, &ignored_ok)) {
        }
    });

    HandleRpcs();
}

void CmdPassImpl::Shutdown() {
    stop = true;
    stop.notify_one();
}

CmdPassImpl::CallData::CallData(CmdPass::AsyncService *service, grpc::ServerCompletionQueue *cq)
    : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE) {
    Proceed();
}

void CmdPassImpl::CallData::Proceed() {
    if (status_ == CREATE) {
        status_ = PROCESS;
        service_->RequestFFmpegCmdExec(&ctx_, &request_, &responder_, cq_, cq_, this);
    } else if (status_ == PROCESS) { /* FFmpeg command processing starts */
        new CallData(service_, cq_);

        std::stringstream ss;
        std::string pipeline_string;
        std::vector<std::pair<std::string, std::string>> committed_config;

        if (request_.obj().empty()) {
            responder_.Finish(response_,
                              Status(grpc::INVALID_ARGUMENT,
                                     FFMPEG_INVALID_COMMAND_STATUS,
                                     FFMPEG_INVALID_COMMAND_MSG),
                              this);

            status_ = FINISH;
            return;
        }

        for (const auto &cmd : request_.obj()) {
            committed_config.push_back(std::make_pair(cmd.cmd_key(), cmd.cmd_val()));
        }

        Config recieved_config = stringPairsToConfig(committed_config);

        if (ffmpeg_generate_pipeline(recieved_config, pipeline_string) != 0) {
            pipeline_string.clear();
            std::cout << "Error generating pipeline" << std::endl; //TODO : need to return as response error code
            //return 1;
        }

        ss << "ffmpeg ";
        ss << pipeline_string;

        pipeline_string = ss.str();

        std::array<char, 128> buffer;
        std::string result;

        FILE *pipe = popen(pipeline_string.c_str(), "r");
        if (!pipe) { /* FFmpeg pipeline/execution failed i.e memory allocation */
            responder_.Finish(response_,
                              Status(grpc::INTERNAL, FFMPEG_APP_EXEC_FAIL_STATUS,
                                     FFMPEG_APP_EXEC_FAIL_MSG),
                              this);

            status_ = FINISH;
            return;
        }

        /* FFmpeg output */
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }

        std::cout << result << std::endl;
        /* FFmpeg output */

        /* FFmpeg pipeline/execution ended */
        if (pclose(pipe) != 0) {
            /* FFmpeg app fail : returns the exit status of FFmpeg app itself */
            responder_.Finish(response_,
                              Status(grpc::UNKNOWN, FFMPEG_COMMAND_FAIL_STATUS,
                                     FFMPEG_COMMAND_FAIL_MSG),
                              this);

            status_ = FINISH;
            return;
            /* FFmpeg app fail : returns the exit status of FFmpeg app itself */
        }
        /* FFmpeg pipeline/execution ended */

        /* FFmpeg successfully executed the commands */
        responder_.Finish(
            response_,
            Status(grpc::OK, FFMPEG_EXEC_OK_STATUS, FFMPEG_EXEC_OK_MSG), this);

        status_ = FINISH;
        return;
    } /* FFmpeg command processing ends */
    else {
        // Once in the FINISH state, deallocate ourselves (CallData).
        delete this;
    }
}

void CmdPassImpl::HandleRpcs() {
    new CallData(&service_, cq_.get());

    void *tag;
    bool ok;
    while (true) {
        cq_->Next(&tag, &ok);

        if (ok) {
            /*
             * calling cq_->Shutdown will render "ok" to be false
             * hence we should terminate the main loop
             */
            static_cast<CallData *>(tag)->Proceed();
        } else
            break;
    }
}
