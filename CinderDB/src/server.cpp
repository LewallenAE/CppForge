#include "cinderdb/server.hpp"
#include "cinderdb/protocol.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <system_error>
#include <sys/socket.h>
#include <unistd.h>
namespace cinderdb { namespace {
void send_all(const int fd,const std::string_view text) { const char* data=text.data(); std::size_t remaining=text.size(); while(remaining>0) { const ssize_t n=::send(fd,data,remaining,MSG_NOSIGNAL); if(n<0) { if(errno==EINTR) continue; return; } if(n==0) return; const auto used=static_cast<std::size_t>(n); data+=used; remaining-=used; } }
void set_receive_timeout(const int fd) { const timeval timeout{0,200000}; if(::setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout))!=0) throw std::system_error(errno,std::generic_category(),"set receive timeout"); }
} 
Server::Server(const unsigned short port,std::string wal_path,const std::size_t workers,const std::size_t queue_capacity) : requested_port_{port},workers_count_{workers},store_{wal_path},connections_{queue_capacity} { if(workers==0) throw std::invalid_argument{"worker count must be positive"}; }
Server::~Server(){ stop(); }
void Server::start() { if(listener_) throw std::logic_error{"server already started"}; const int raw=::socket(AF_INET,SOCK_STREAM,0); if(raw<0) throw std::system_error(errno,std::generic_category(),"socket"); listener_.reset(raw); int enabled=1; if(::setsockopt(raw,SOL_SOCKET,SO_REUSEADDR,&enabled,sizeof(enabled))!=0) throw std::system_error(errno,std::generic_category(),"setsockopt SO_REUSEADDR"); sockaddr_in address{}; address.sin_family=AF_INET; address.sin_addr.s_addr=htonl(INADDR_LOOPBACK); address.sin_port=htons(requested_port_); if(::bind(raw,reinterpret_cast<const sockaddr*>(&address),sizeof(address))!=0) throw std::system_error(errno,std::generic_category(),"bind"); if(::listen(raw,64)!=0) throw std::system_error(errno,std::generic_category(),"listen"); sockaddr_in actual{}; socklen_t length=sizeof(actual); if(::getsockname(raw,reinterpret_cast<sockaddr*>(&actual),&length)!=0) throw std::system_error(errno,std::generic_category(),"getsockname"); bound_port_=ntohs(actual.sin_port); stopping_.store(false); workers_.reserve(workers_count_); for(std::size_t i=0;i<workers_count_;++i) workers_.emplace_back([this]{worker_loop();}); accept_thread_=std::thread([this]{accept_loop();}); }
void Server::stop() noexcept { if(stopping_.exchange(true)) return; if(listener_) ::shutdown(listener_.get(),SHUT_RDWR); connections_.close(); if(accept_thread_.joinable()) accept_thread_.join(); for(auto& worker:workers_) if(worker.joinable()) worker.join(); listener_.reset(); }
unsigned short Server::port() const noexcept { return bound_port_; }
void Server::accept_loop() noexcept { while(!stopping_.load()) { sockaddr_in peer{}; socklen_t length=sizeof(peer); const int accepted=::accept(listener_.get(),reinterpret_cast<sockaddr*>(&peer),&length); if(accepted<0) { if(errno==EINTR) continue; if(stopping_.load()) break; continue; } try { set_receive_timeout(accepted); } catch(...) { ::close(accepted); continue; } if(!connections_.push(UniqueFd{accepted})) break; } }
void Server::worker_loop() noexcept { while(auto client=connections_.pop()) handle_client(std::move(*client)); }
void Server::handle_client(UniqueFd client) noexcept { std::array<char,1024> chunk{}; std::string buffer; while(!stopping_.load()) { const ssize_t count=::recv(client.get(),chunk.data(),chunk.size(),0); if(count==0) return; if(count<0) { if(errno==EINTR) continue; if(errno==EAGAIN || errno==EWOULDBLOCK) continue; return; } buffer.append(chunk.data(),static_cast<std::size_t>(count)); if(buffer.size()>max_request_line_size) { send_all(client.get(),"ERROR request too large\n"); return; } for(;;) { const auto newline=buffer.find('\n'); if(newline==std::string::npos) break; std::string line=buffer.substr(0,newline); buffer.erase(0,newline+1); if(!line.empty() && line.back()=='\r') line.pop_back(); const auto parsed=parse_command(line); if(!parsed.command) { send_all(client.get(),"ERROR "+parsed.error+"\n"); continue; } try { const auto& command=*parsed.command; if(command.type==CommandType::put) { store_.put(command.key,command.value); send_all(client.get(),"OK\n"); } else if(command.type==CommandType::erase) { store_.erase(command.key); send_all(client.get(),"OK\n"); } else { const auto value=store_.get(command.key); send_all(client.get(),value ? "VALUE "+*value+"\n" : "NOT_FOUND\n"); } } catch(const std::exception&) { send_all(client.get(),"ERROR storage failure\n"); } } } }
}  // namespace cinderdb
