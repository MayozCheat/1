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

// �Զ������ݽṹ
struct CustomData {
    DWORD dwValue;
    float x, y, z;
    std::string name;
    // ... ������Ա ...
};

//�����л�
std::vector<CustomData> deserialize(const std::vector<char>& buffer) {
    std::vector<CustomData> dataArray;
    size_t i = 0;

    while (i < buffer.size()) {
        CustomData data;

        // �����л� DWORD
        std::memcpy(&data.dwValue, &buffer[i], sizeof(data.dwValue));
        i += sizeof(data.dwValue);

        // �����л� float ����
        std::memcpy(&data.x, &buffer[i], sizeof(data.x));
        i += sizeof(data.x);
        std::memcpy(&data.y, &buffer[i], sizeof(data.y));
        i += sizeof(data.y);
        std::memcpy(&data.z, &buffer[i], sizeof(data.z));
        i += sizeof(data.z);

        // �����л��ַ���
        DWORD nameLength;
        std::memcpy(&nameLength, &buffer[i], sizeof(nameLength));
        i += sizeof(nameLength);
        data.name.assign(buffer.begin() + i, buffer.begin() + i + nameLength);
        i += nameLength;

        dataArray.push_back(data);
    }

    return dataArray;
}


// ǰ�˻Ự��
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
            std::cerr << "ǰ�����ӽ��ܴ���: " << ec.message() << "\n";
            return;
        }
        // ���ӽ�������߼�
    }


    void on_write(beast::error_code ec, std::size_t) {
        if (ec) {
            std::cerr << "������ǰ�����ݴ���: " << ec.message() << "\n";
            return;
        }
        // ���ݷ��ͺ���߼�
    }
};

//�ͻ��˻Ự��
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
            std::cerr << "�ͻ���ͨѶ���ӽ��ܴ���: " << ec.message() << "\n";
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
            std::cerr << "�ͻ���ͨѶ���ݽ��մ���: " << ec.message() << "\n";
            return;
        }

        //�������
        {
            // ��ȡ����
            auto bufferData = buffer_.data();
            const char* dataPtr = static_cast<const char*>(bufferData.data());
            std::size_t dataSize = bufferData.size();

            std::vector<char> receivedData(dataPtr, dataPtr + dataSize);

            // �����л�����
            std::vector<CustomData> deserializedData = deserialize(receivedData);

            // ��� buffer �Ա��´ζ�ȡ
            buffer_.consume(buffer_.size());

            // ��ӡ����
            for (const auto& data : deserializedData) {
                std::cout << "���յ�����: " << data.name << " ("
                    << data.x << ", " << data.y << ", " << data.z << "), dwValue: " << data.dwValue << "\n";
            }


            read(); // ������ȡ����
        }



        // ������յ�������
        // ���յ������� CustomData ����
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
                  std::cout << "��ҷ��ʹ���:" << wanjia << std::endl;
              }
              else if (data_array[i - 1].dwValue == 4)
              {
                  rongqi += 1;
                  std::cout << "RQ���ʹ���:" << rongqi << std::endl;
              }
              else if (data_array[i - 1].dwValue == 3)
              {
                  wuzi += 1;
                  std::cout << "�����������ʷ��ʹ���:" << wuzi << std::endl;
              }*/
              // buffer_.consume(buffer_.size());
        // read(); // ������ȡ
        }




    }
};

// ��������
class Server {
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::unordered_map<std::string, std::shared_ptr<FrontendSession>> frontend_sessions_;//�ͻ���ID��sessionӳ��

public:
    Server(net::io_context& ioc, unsigned short port) : ioc_(ioc), acceptor_(ioc, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

    //���  �ͻ�ID��ǰ��sessionӳ��
    void set_frontend_session(const std::string& client_id, std::shared_ptr<FrontendSession> session)
    {
        frontend_sessions_[client_id] = session;
    }

    //���ݿͻ�ID����ǰ��session
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
                if (!ec) //���տͻ��˶Ի��ɹ�
                {
                    // ʹ�� UUID ����������һ��Ψһ�Ŀͻ��� ID
                    boost::uuids::uuid uuid = boost::uuids::random_generator()();
                    std::string client_id = boost::uuids::to_string(uuid);

                    auto frontend_session = get_frontend_session(client_id);
                    if (!frontend_session)
                    {
                        // ���û��Ϊ�� ID ����ǰ�˻Ự������һ���µ�
                        frontend_session = std::make_shared<FrontendSession>(tcp::socket(ioc_));
                        std::cout << "frontend_session:" << frontend_session << std::endl;
                        frontend_session->start();//ǰ��-�����ͨѶ����

                        set_frontend_session(client_id, frontend_session);
                    }

                    // Ϊÿ���ͻ��˻Ự����һ�����߳�
                    std::thread([this, socket = std::move(socket), frontend_session]() mutable
                    {
                        auto session = std::make_shared<Session>(std::move(socket), frontend_session);
                        session->start();//�ͻ���-�����ͨѶ����

                        this->ioc_.run();
                    }).detach();
                }
                else
                {
                    std::cerr << "���տͻ��˶Ի�ʧ��: " << ec.message() << "\n";
                }

                do_accept(); // ��������������
            });
    }
};

int main() {
    try {
        net::io_context ioc;
        unsigned short port = 8080;

        Server server(ioc, port);

        std::cout << "�����������˿�: " << port << "\n";

        ioc.run(); // �����߳������� IO �������Դ����첽�¼�
    }
    catch (const std::exception& e) {
        std::cerr << "�쳣: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}