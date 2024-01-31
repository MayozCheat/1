#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include <string>
#include <memory>
#include <unordered_map>
#include <thread>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// 自定义数据结构
struct CustomData {
    DWORD dwValue;
    float x, y, z;
    std::string name;
    // ... 其他成员 ...
};

//返序列化
std::vector<CustomData> deserialize(const std::vector<char>& buffer) {
    std::vector<CustomData> dataArray;
    size_t i = 0;

    while (i < buffer.size()) {
        CustomData data;

        // 反序列化 DWORD
        std::memcpy(&data.dwValue, &buffer[i], sizeof(data.dwValue));
        i += sizeof(data.dwValue);

        // 反序列化 float 坐标
        std::memcpy(&data.x, &buffer[i], sizeof(data.x));
        i += sizeof(data.x);
        std::memcpy(&data.y, &buffer[i], sizeof(data.y));
        i += sizeof(data.y);
        std::memcpy(&data.z, &buffer[i], sizeof(data.z));
        i += sizeof(data.z);

        // 反序列化字符串
        DWORD nameLength;
        std::memcpy(&nameLength, &buffer[i], sizeof(nameLength));
        i += sizeof(nameLength);
        data.name.assign(buffer.begin() + i, buffer.begin() + i + nameLength);
        i += nameLength;

        dataArray.push_back(data);
    }

    return dataArray;
}


// 前端会话类
class FrontendSession : public std::enable_shared_from_this<FrontendSession> {
    websocket::stream<tcp::socket> ws_;

public:
    explicit FrontendSession(tcp::socket socket) : ws_(std::move(socket)) {}

    //
    void start()
    {
        ws_.async_accept(
            beast::bind_front_handler(
                &FrontendSession::on_accept,
                shared_from_this()));
    }


    void send_data_to_frontend(const CustomData& data) {
        ws_.binary(true);
        ws_.async_write(
            net::buffer(&data, sizeof(data)),
            beast::bind_front_handler(
                &FrontendSession::on_write,
                shared_from_this()));
    }

private:
    void on_accept(beast::error_code ec) {
        if (ec) {
            std::cerr << "前端连接接受错误: " << ec.message() << "\n";
            return;
        }
        // 连接建立后的逻辑
    }


    void on_write(beast::error_code ec, std::size_t) {
        if (ec) {
            std::cerr << "发送至前端数据错误: " << ec.message() << "\n";
            return;
        }
        // 数据发送后的逻辑
    }
};

//客户端会话类
class Session : public std::enable_shared_from_this<Session> {
    websocket::stream<tcp::socket> ws_;
    std::shared_ptr<FrontendSession> frontend_session_;
    beast::flat_buffer buffer_;

public:
    Session(tcp::socket socket, std::shared_ptr<FrontendSession> frontend_session)
        : ws_(std::move(socket)), frontend_session_(frontend_session) {}



    void start()
    {
        ws_.async_accept(
            beast::bind_front_handler(
                &Session::on_accept,
                shared_from_this()));
    }

private:
    void on_accept(beast::error_code ec)
    {
        if (ec) {
            std::cerr << "客户端通讯连接接受错误: " << ec.message() << "\n";
            return;
        }
        read();
    }


    void read() {
        ws_.binary(true);
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &Session::on_read,
                shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t) {
        if (ec)
        {
            std::cerr << "客户端通讯数据接收错误: " << ec.message() << "\n";
            return;
        }

        //输出测试
        {
            // 提取数据
            auto bufferData = buffer_.data();
            const char* dataPtr = static_cast<const char*>(bufferData.data());
            std::size_t dataSize = bufferData.size();

            std::vector<char> receivedData(dataPtr, dataPtr + dataSize);

            // 反序列化数据
            std::vector<CustomData> deserializedData = deserialize(receivedData);

            // 清空 buffer 以备下次读取
            buffer_.consume(buffer_.size());

            // 打印数据
            for (const auto& data : deserializedData) {
                std::cout << "接收到数据: " << data.name << " ("
                    << data.x << ", " << data.y << ", " << data.z << "), dwValue: " << data.dwValue << "\n";
            }


            read(); // 继续读取数据
        }



        // 处理接收到的数据
        // 接收到完整的 CustomData 数组
        {
            /*  static int wanjia = 0, wuzi = 0, rongqi = 0;
              CustomData* data_array = reinterpret_cast<CustomData*>(buffer_.data().data());
              std::size_t data_count = buffer_.size() / sizeof(CustomData);

              std::size_t i = 0;
              for (i; i < data_count; ++i) {
                  frontend_session_->send_data_to_frontend(data_array[i]);
              }
              if (data_array[i - 1].dwValue <= 1)
              {
                  wanjia += 1;
                  std::cout << "玩家发送次数:" << wanjia << std::endl;
              }
              else if (data_array[i - 1].dwValue == 4)
              {
                  rongqi += 1;
                  std::cout << "RQ发送次数:" << rongqi << std::endl;
              }
              else if (data_array[i - 1].dwValue == 3)
              {
                  wuzi += 1;
                  std::cout << "物资物资物资发送次数:" << wuzi << std::endl;
              }*/
              // buffer_.consume(buffer_.size());
        // read(); // 继续读取
        }




    }
};

// 服务器类
class Server {
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::unordered_map<std::string, std::shared_ptr<FrontendSession>> frontend_sessions_;//客户端ID与session映射

public:
    Server(net::io_context& ioc, unsigned short port) : ioc_(ioc), acceptor_(ioc, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

    //添加  客户ID与前端session映射
    void set_frontend_session(const std::string& client_id, std::shared_ptr<FrontendSession> session)
    {
        frontend_sessions_[client_id] = session;
    }

    //根据客户ID返回前端session
    std::shared_ptr<FrontendSession> get_frontend_session(const std::string& client_id)
    {
        auto it = frontend_sessions_.find(client_id);
        if (it != frontend_sessions_.end()) {
            return it->second;
        }
        return nullptr;
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(
            [this](beast::error_code ec, tcp::socket socket)
            {
                if (!ec) //接收客户端对话成功
                {
                    // 使用 UUID 生成器创建一个唯一的客户端 ID
                    boost::uuids::uuid uuid = boost::uuids::random_generator()();
                    std::string client_id = boost::uuids::to_string(uuid);

                    auto frontend_session = get_frontend_session(client_id);
                    if (!frontend_session)
                    {
                        // 如果没有为此 ID 创建前端会话，创建一个新的
                        frontend_session = std::make_shared<FrontendSession>(tcp::socket(ioc_));
                        std::cout << "frontend_session:" << frontend_session << std::endl;
                        frontend_session->start();//前端-服务端通讯启动

                        set_frontend_session(client_id, frontend_session);
                    }

                    // 为每个客户端会话创建一个新线程
                    std::thread([this, socket = std::move(socket), frontend_session]() mutable
                    {
                        auto session = std::make_shared<Session>(std::move(socket), frontend_session);
                        session->start();//客户端-服务端通讯启动

                        this->ioc_.run();
                    }).detach();
                }
                else
                {
                    std::cerr << "接收客户端对话失败: " << ec.message() << "\n";
                }

                do_accept(); // 继续接受新连接
            });
    }
};

int main() {
    try {
        net::io_context ioc;
        unsigned short port = 8080;

        Server server(ioc, port);

        std::cout << "服务器监听端口: " << port << "\n";

        ioc.run(); // 在主线程中运行 IO 上下文以处理异步事件
    }
    catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}