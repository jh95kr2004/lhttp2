#ifndef _frame_H_
#define _frame_H_

#include <string>
#include <cstdint>

#include "buffer/Buffer.h"
#include "hpack/hpack.h"
#include "settings.h"
#include "error.h"

namespace lhttp2 {
    class Frame;                  // Header of frame

    /*
        Classes below are payload classes.
        All of them inherits frame_payload class
        and is implemented according to the type of frame.
    */
    class DataFrame;             // DATA (0x0)
    class HeadersFrame;          // HEADERS (0x1)
    class PriorityFrame;         // PRIORITY (0x2)
    class RSTStreamFrame;        // RST_STREAM (0x3)
    class SettingsFrame;         // SETTINGS (0x4)
    class PushPromisFrame;       // PUSH_PROMISE (0x5)
    class PingFrame;             // PING (0x6)
    class GoawayFrame;           // GOAWAY (0x7)
    class WindowUpdateFrame;     // WINDOW_UPDATE (0x8)
    class ContinuationFrame;     // CONTINUATION (0x9)

    /*
        ### Header of frame ###

        All frames begin with a fixed 9-octet header followed by a variable-length payload.

        +-----------------------------------------------+
        |                 Length (24)                   |
        +---------------+---------------+---------------+
        |   Type (8)    |   Flags (8)   |
        +-+-------------+---------------+-------------------------------+
        |R|                 Stream Identifier (31)                      |
        +=+=============================================================+
        |                   Frame Payload (0...)                      ...
        +---------------------------------------------------------------+
    */
    class Frame {
    public:
        typedef enum _FRAME_TYPE{
            TYPE_DATA_FRAME = 0,
            TYPE_HEADERS_FRAME,
            TYPE_PRIORITY_FRAME,
            TYPE_RST_STREAM_FRAME,
            TYPE_SETTINGS_FRAME,
            TYPE_PUSH_PROMISE_FRAME,
            TYPE_PING_FRAME,
            TYPE_GOAWAY_FRAME,
            TYPE_WINDOW_UPDATE_FRAME,
            TYPE_CONTINUATION_FRAME,
        } FRAME_TYPE;

        typedef enum _FRAME_FLAG {
            FLAG_ACK = 0x1,
            FLAG_END_STREAM = 0x1,
            FLAG_END_HEADERS = 0x4,
            FLAG_PADDED = 0x8,
            FLAG_PRIORITY = 0x20,
        } FRAME_FLAG;

        virtual ~Frame() = 0;

        const uint32_t length() const;
        const FRAME_TYPE type() const;
        const uint8_t flags() const;
        const uint32_t stream_id() const;
        const bool reserved() const;

        void set_flags(uint8_t flags);
        void clear_flags(uint8_t flags);
        bool has_flags(uint8_t flags) const;

        void set_stream_id(uint32_t streamId);

        static Frame* RecvFrame(const int fd, hpack::Table& hpack_table, bool debug = false);
        static int SendFrame(const int fd, Frame* frame, hpack::Table& hpack_table, bool debug = false);
        static const std::string GetFrameTypeName(FRAME_TYPE type);

    protected:
        Buffer* EncodeFrame(hpack::Table& hpack_table);
        virtual Buffer* EncodeFramePayload(hpack::Table& hpack_table) = 0;
        virtual bool DecodeFramePayload(const char* buff, const int len, hpack::Table& hpack_table) = 0;
        virtual void UpdateLength() = 0;

        uint32_t length_ = 0;
        FRAME_TYPE type_ = TYPE_SETTINGS_FRAME;
        uint8_t flags_ = 0;
        uint32_t stream_id_ = 0;
        bool reserved_ = false;
    };

    /*
        ### DATA FRAME ###

        DATA frames (type=0x0) convey arbitrary, variable-length sequences of
        octets associated with a stream.  One or more DATA frames are used,
        for instance, to carry HTTP request or response payloads.

        DATA frames MAY also contain padding.  Padding can be added to DATA
        frames to obscure the size of messages.  Padding is a security feature.

        +---------------+
        |Pad Length? (8)|
        +---------------+-----------------------------------------------+
        |                            Data (*)                         ...
        +---------------------------------------------------------------+
        |                           Padding (*)                       ...
        +---------------------------------------------------------------+
    */
    class DataFrame final : public Frame {
    public:
        DataFrame();
        DataFrame(Buffer data, uint8_t pad_length = 0);
        ~DataFrame();

        const uint8_t pad_length() const;
        const Buffer& data() const;

        void set_pad_length(uint8_t pad_length);
        void set_data(Buffer& data);

        bool has_end_stream_flag() const;
        bool has_padded_flag() const;

        void set_end_stream_flag();
        void set_padded_flag();

        void clear_end_stream_flag();
        void clear_padded_flag();

    private:
        Buffer* EncodeFramePayload(hpack::Table& hpack_table) override;
        bool DecodeFramePayload(const char* buff, const int len, hpack::Table& hpack_table) override;
        void UpdateLength() override;

        uint8_t pad_length_ = 0;
        Buffer data_;
    };

    /*
        ### HEADERS FRAME ###

        The HEADERS frame (type=0x1) is used to open a stream,
        and additionally carries a header block fragment.  HEADERS frames can
        be sent on a stream in the "idle", "reserved (local)", "open", or
        "half-closed (remote)" state.

        +---------------+
        |Pad Length? (8)|
        +-+-------------+-----------------------------------------------+
        |E|                 Stream Dependency? (31)                     |
        +-+-------------+-----------------------------------------------+
        |  Weight? (8)  |
        +-+-------------+-----------------------------------------------+
        |                   Header Block Fragment (*)                 ...
        +---------------------------------------------------------------+
        |                           Padding (*)                       ...
        +---------------------------------------------------------------+
    */
    class HeadersFrame final : public Frame {
    public:
        HeadersFrame();
        HeadersFrame(std::vector<hpack::HeaderFieldRepresentation> header_list, hpack::Table& hpack_table, uint8_t pad_length);
        HeadersFrame(std::vector<hpack::HeaderFieldRepresentation> header_list, hpack::Table& hpack_table, bool exclusive, uint32_t stream_dependency, uint8_t weight, uint8_t pad_length = 0);
        ~HeadersFrame();

        const uint8_t pad_length() const;
        const bool exclusive() const;
        const uint32_t stream_dependency() const;
        const uint8_t weight() const;
        const std::vector<hpack::HeaderFieldRepresentation>& header_list() const;
        const Buffer& header_block_fragment() const;

        void set_pad_length(uint8_t pad_length);
        void set_exclusive(bool exclusive);
        void set_stream_dependency(uint32_t stream_dependency);
        void set_weight(uint8_t weight);
        void set_header_list(std::vector<hpack::HeaderFieldRepresentation> header_list, hpack::Table& hpack_table);

        void update_header_block_fragment(hpack::Table& hpack_table);

        bool has_end_stream_flag() const;
        bool has_end_headers_flag() const;
        bool has_padded_flag() const;
        bool has_priority_flag() const;

        void set_end_stream_flag();
        void set_end_headers_flag();
        void set_padded_flag();
        void set_priority_flag();

        void clear_end_stream_flag();
        void clear_end_headers_flag();
        void clear_padded_flag();
        void clear_priority_flag();

    private:
        Buffer* EncodeFramePayload(hpack::Table& hpack_table) override;
        bool DecodeFramePayload(const char* buff, const int len, hpack::Table& hpack_table) override;
        void UpdateLength() override;

        uint8_t pad_length_ = 0;
        bool exclusive_ = false;
        uint32_t stream_dependency_ = 0;
        uint8_t weight_ = 0;
        std::vector<hpack::HeaderFieldRepresentation> header_list_;
        Buffer header_;
    };

    /*
        ### PRIORITY FRAME ###

        The PRIORITY frame (type=0x2) specifies the sender-advised priority
        of a stream.  It can be sent in any stream state, including idle or 
        closed streams.

        +-+-------------------------------------------------------------+
        |E|                  Stream Dependency (31)                     |
        +-+-------------+-----------------------------------------------+
        |   Weight (8)  |
        +-+-------------+
    */
    class PriorityFrame final : public Frame {
    public:
        PriorityFrame();
        PriorityFrame(bool exclusive, uint32_t stream_dependency, uint8_t weight);
        ~PriorityFrame();

        const bool exclusive() const;
        const uint32_t stream_dependency() const;
        const uint8_t weight() const;

        void set_exclusive(bool exclusive);
        void set_stream_dependency(uint32_t stream_dependency);
        void set_weight(uint8_t weight);

    private:
        Buffer* EncodeFramePayload(hpack::Table& hpack_table) override;
        bool DecodeFramePayload(const char* buff, const int len, hpack::Table& hpack_table) override;
        void UpdateLength() override;

        bool exclusive_ = false;
        uint32_t stream_dependency_ = 0;
        uint8_t weight_ = 0;
        Buffer header_;
    };

    /*
        ### RST_STREAM FRAME ###

        The RST_STREAM frame (type=0x3) allows for immediate termination of a
        stream.  RST_STREAM is sent to request cancellation of a stream or to
        indicate that an error condition has occurred.

        +---------------------------------------------------------------+
        |                        Error Code (32)                        |
        +---------------------------------------------------------------+
    */
    class RSTStreamFrame final : public Frame {
    public:
        RSTStreamFrame();
        RSTStreamFrame(uint32_t error_code);
        ~RSTStreamFrame();

        const uint32_t error_code() const;
        void set_error_code(uint32_t error_code);

    private:
        Buffer* EncodeFramePayload(hpack::Table& hpack_table) override;
        bool DecodeFramePayload(const char* buff, const int len, hpack::Table& hpack_table) override;
        void UpdateLength() override;

        uint32_t error_code_ = 0;
    };

    /*
        ### SETTINGS FRAME ###

        The SETTINGS frame (type=0x4) conveys configuration parameters that
        affect how endpoints communicate, such as preferences and constraints
        on peer behavior.  The SETTINGS frame is also used to acknowledge the
        receipt of those parameters.  Individually, a SETTINGS parameter can
        also be referred to as a "setting".

        +-------------------------------+
        |       Identifier (16)         |
        +-------------------------------+-------------------------------+
        |                        Value (32)                             |
        +---------------------------------------------------------------+
    */
    class SettingsFrame final : public Frame {
    public:
        typedef enum _SETTINGS_PARAMETERS {
            SETTINGS_HEADER_TABLE_SIZE = 0x1,
            SETTINGS_ENABLE_PUSH,
            SETTINGS_MAX_CONCURRENT_STREAMS,
            SETTINGS_INITIAL_WINDOW_SIZE,
            SETTINGS_MAX_FRAME_SIZE,
            SETTINGS_MAX_HEADER_LIST_SIZE,
        } SETTINGS_PARAMETERS;

        SettingsFrame();
        SettingsFrame(lhttp2::Settings settings);
        ~SettingsFrame();

        const lhttp2::Settings& settings() const;
        void set_settings(lhttp2::Settings& settings);

        bool has_ack_flag();
        void set_ack_flag();
        void clear_ack_flag();

    private:
        Buffer* EncodeFramePayload(hpack::Table& hpack_table) override;
        bool DecodeFramePayload(const char* buff, const int len, hpack::Table& hpack_table) override;
        void UpdateLength() override;

        lhttp2::Settings settings_;
    };

    /*
        ### PUSH_PROMISE FRAME ###

        The PUSH_PROMISE frame (type=0x5) is used to notify the peer endpoint
        in advance of streams the sender intends to initiate.  The PUSH_PROMISE
        frame includes the unsigned 31-bit identifier of the stream the endpoint
        plans to create along with a set of headers that provide additional
        context for the stream.

        +---------------+
        |Pad Length? (8)|
        +-+-------------+-----------------------------------------------+
        |R|                  Promised Stream ID (31)                    |
        +-+-----------------------------+-------------------------------+
        |                   Header Block Fragment (*)                 ...
        +---------------------------------------------------------------+
        |                           Padding (*)                       ...
        +---------------------------------------------------------------+
    */
    class PushPromisFrame final : public Frame {
    public:
        PushPromisFrame();
        PushPromisFrame(uint32_t promised_stream_id, Buffer header_block_fragment, uint8_t pad_length = 0);
        ~PushPromisFrame();

        const uint8_t pad_length() const;
        const bool reserved() const;
        const uint32_t promised_stream_id() const;
        const Buffer& header_block_fragment() const;

        void set_pad_length(uint8_t pad_length);
        void set_reserved(bool reserved);
        void set_promised_stream_id(uint32_t promised_stream_id);
        void set_header_block_fragment(Buffer& header_block_fragment);

        bool has_end_headers_flag();
        bool has_padded_flag();

        void set_end_headers_flag();
        void set_padded_flag();
        
        void clear_end_headers_flag();
        void clear_padded_flag();

    private:
        Buffer* EncodeFramePayload(hpack::Table& hpack_table) override;
        bool DecodeFramePayload(const char* buff, const int len, hpack::Table& hpack_table) override;
        void UpdateLength() override;

        uint8_t pad_length_ = 0;
        bool reserved_ = false;
        uint32_t promised_stream_id_;
        Buffer header_block_fragment_;
    };

    /*
        ### PING FRAME ###

        The PING frame (type=0x6) is a mechanism for measuring a minimal
        round-trip time from the sender, as well as determining whether an
        idle connection is still functional.  PING frames can be sent from
        any endpoint.

        +---------------------------------------------------------------+
        |                                                               |
        |                      Opaque Data (64)                         |
        |                                                               |
        +---------------------------------------------------------------+
    */
    class PingFrame final : public Frame {
    public:
        PingFrame();
        PingFrame(uint64_t opaque_data);
        ~PingFrame();

        const uint64_t opaque_data() const;
        void set_opaque_data(uint64_t opaque_data);

        bool has_ack_flag();
        void set_ack_flag();
        void clear_ack_flag();

    private:
        Buffer* EncodeFramePayload(hpack::Table& hpack_table) override;
        bool DecodeFramePayload(const char* buff, const int len, hpack::Table& hpack_table) override;
        void UpdateLength() override;

        uint64_t opaque_data_ = 0;
    };

    /*
        ### GOAWAY FRAME ###

        The GOAWAY frame (type=0x7) is used to initiate shutdown of a
        connection or to signal serious error conditions.  GOAWAY allows an
        endpoint to gracefully stop accepting new streams while still
        finishing processing of previously established streams.  This enables
        administrative actions, like server maintenance.

        +-+-------------------------------------------------------------+
        |R|                  Last-Stream-ID (31)                        |
        +-+-------------------------------------------------------------+
        |                      Error Code (32)                          |
        +---------------------------------------------------------------+
        |                  Additional Debug Data (*)                    |
        +---------------------------------------------------------------+
    */
    class GoawayFrame final : public Frame {
    public:
        GoawayFrame();
        GoawayFrame(uint32_t last_stream_id, uint32_t error_code, Buffer additional_debug_data);
        ~GoawayFrame();

        const bool reserved() const;
        const uint32_t last_stream_id() const;
        const uint32_t error_code() const;
        const Buffer& additional_debug_data() const;

        void set_reserved(bool reserved);
        void set_last_stream_id(uint32_t last_stream_id);
        void set_error_code(uint32_t error_code);
        void set_additional_debug_data(Buffer& additional_debug_data);

    private:
        Buffer* EncodeFramePayload(hpack::Table& hpack_table) override;
        bool DecodeFramePayload(const char* buff, const int len, hpack::Table& hpack_table) override;
        void UpdateLength() override;

        bool reserved_ = false;
        uint32_t last_stream_id_;
        uint32_t error_code_;
        Buffer additional_debug_data_;
    };

    /*
        ### WINDOW_UPDATE FRAME ###

        The WINDOW_UPDATE frame (type=0x8) is used to implement flow control.

        +-+-------------------------------------------------------------+
        |R|              Window Size Increment (31)                     |
        +-+-------------------------------------------------------------+
    */
    class WindowUpdateFrame final : public Frame {
    public:
        WindowUpdateFrame();
        WindowUpdateFrame(uint32_t window_size_increment);
        ~WindowUpdateFrame();

        const bool reserved() const;
        const uint32_t window_size_increment() const;

        void set_reserved(bool reserved);
        void set_window_size_increment(int window_size_increment);

    private:
        Buffer* EncodeFramePayload(hpack::Table& hpack_table) override;
        bool DecodeFramePayload(const char* buff, const int len, hpack::Table& hpack_table) override;
        void UpdateLength() override;

        bool reserved_ = false;
        uint32_t window_size_increment_;
    };

    /*
        ### CONTINUATION FRAME ###

        The CONTINUATION frame (type=0x9) is used to continue a sequence of
        header block fragments. Any number of CONTINUATION frames can be sent, 
        as long as the preceding frame is on the same stream and is a HEADERS, 
        PUSH_PROMISE, or CONTINUATION frame without the END_HEADERS flag set.

        +---------------------------------------------------------------+
        |                   Header Block Fragment (*)                 ...
        +---------------------------------------------------------------+
    */
    class ContinuationFrame final : public Frame {
    public:
        ContinuationFrame();
        ContinuationFrame(Buffer& header_block_fragment);
        ~ContinuationFrame();

        const Buffer& header_block_fragment() const;
        void set_header_block_fragment(Buffer& header_block_fragment);

        bool has_end_headers_flag();
        void set_end_headers_flag();
        void clear_end_headers_flag();

    private:
        Buffer* EncodeFramePayload(hpack::Table& hpack_table) override;
        bool DecodeFramePayload(const char* buff, const int len, hpack::Table& hpack_table) override;
        void UpdateLength() override;

        Buffer header_block_fragment_;
    };
};

#endif
