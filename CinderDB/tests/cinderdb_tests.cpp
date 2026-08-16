#include "cinderdb/fd.hpp"
#include "cinderdb/protocol.hpp"
#include "cinderdb/server.hpp"
#include "cinderdb/store.hpp"
#include "cinderdb/wal.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <future>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>
namespace { int failures=0; void check(bool ok,std::string_view text){if(!ok){std::cerr<<"FAIL: "<<text<<'\n';++failures;}}
std::string temp_wal(std::string_view tag){ return "/tmp/cinderdb-"+std::string{tag}+"-"+std::to_string(::getpid())+".wal"; }
void send_all(int fd,std::string_view data){while(!data.empty()){ssize_t n=::send(fd,data.data(),data.size(),MSG_NOSIGNAL);if(n<=0) throw std::runtime_error{"send failed"};data.remove_prefix(static_cast<std::size_t>(n));}}
int connect_to(unsigned short port){int fd=::socket(AF_INET,SOCK_STREAM,0);if(fd<0)throw std::runtime_error{"socket"};sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(port);if(::connect(fd,reinterpret_cast<sockaddr*>(&a),sizeof(a))!=0){::close(fd);throw std::runtime_error{"connect"};}return fd;}
std::string response(int fd){std::string out;char ch=0;while(true){ssize_t n=::recv(fd,&ch,1,0);if(n<=0)throw std::runtime_error{"recv"};if(ch=='\n')return out;out+=ch;}}
std::string command(unsigned short port,std::string_view text){cinderdb::UniqueFd fd{connect_to(port)};send_all(fd.get(),text);return response(fd.get());}
void test_protocol(){ using cinderdb::CommandType; auto put=cinderdb::parse_command("PUT a hello world");check(put.command&&put.command->type==CommandType::put&&put.command->value=="hello world","parse multiword PUT");check(cinderdb::parse_command("GET a").command.has_value(),"parse GET");check(cinderdb::parse_command("DELETE a").command.has_value(),"parse DELETE");check(!cinderdb::parse_command("NOPE a").command,"reject unknown");check(!cinderdb::parse_command("GET").command,"reject missing key");check(!cinderdb::parse_command("PUT a").command,"reject missing value");check(!cinderdb::parse_command("GET a b").command,"reject extra token");check(!cinderdb::parse_command("").command,"reject empty");check(!cinderdb::parse_command(std::string(cinderdb::max_request_line_size+1,'x')).command,"reject oversized"); }
void test_fd(){int pair[2]{};check(::pipe(pair)==0,"create pipe");cinderdb::UniqueFd first{pair[0]};cinderdb::UniqueFd second{std::move(first)};check(!first&&second,"move descriptor ownership");::close(pair[1]);}
void test_store_wal(){const auto path=temp_wal("store");std::filesystem::remove(path);{cinderdb::KeyValueStore store{path};store.put("a","one");store.put("a","two");store.put("b","three");store.erase("b");std::vector<std::thread> writers;for(std::size_t t=0;t<4;++t)writers.emplace_back([&store,t]{for(std::size_t i=0;i<20;++i)store.put("c"+std::to_string(t)+"_"+std::to_string(i),"v");});for(auto& writer:writers)writer.join();check(store.get("a")==std::optional<std::string>{"two"},"store overwrite");check(!store.get("b"),"store delete");check(store.size()==81,"concurrent writes retain every key");}{cinderdb::KeyValueStore recovered{path};check(recovered.get("a")==std::optional<std::string>{"two"},"WAL restart recovery");check(!recovered.get("b"),"WAL delete recovery");} {int fd=::open(path.c_str(),O_WRONLY|O_APPEND);const char tail[2]{'C','D'};check(::write(fd,tail,sizeof(tail))==static_cast<ssize_t>(sizeof(tail)),"write incomplete tail");::close(fd);} {cinderdb::KeyValueStore recovered{path};check(recovered.get("a")==std::optional<std::string>{"two"},"ignore incomplete final WAL tail");} std::filesystem::remove(path);const auto corrupt=temp_wal("corrupt");std::filesystem::remove(corrupt);{cinderdb::KeyValueStore store{corrupt};store.put("safe","value");}{int fd=::open(corrupt.c_str(),O_WRONLY);const char bad{'X'};check(::write(fd,&bad,1)==1,"corrupt WAL header");::close(fd);}bool rejected=false;try{cinderdb::KeyValueStore broken{corrupt};}catch(const std::exception&){rejected=true;}check(rejected,"reject corrupt WAL record instead of silently replaying");std::filesystem::remove(corrupt);}
void test_server_and_stress(){const auto path=temp_wal("server");std::filesystem::remove(path);unsigned short first_port=0;{cinderdb::Server server{0,path,4,16};server.start();first_port=server.port();check(command(first_port,"PUT user Anthony\n")=="OK","TCP PUT");check(command(first_port,"GET user\n")=="VALUE Anthony","TCP GET");check(command(first_port,"DELETE user\n")=="OK","TCP DELETE");check(command(first_port,"GET user\n")=="NOT_FOUND","TCP missing");check(command(first_port,"BAD\n").starts_with("ERROR"),"malformed client survives");std::atomic<bool> correct{true};std::vector<std::thread> clients;for(std::size_t c=0;c<8;++c)clients.emplace_back([&,c]{try{cinderdb::UniqueFd fd{connect_to(first_port)};for(std::size_t i=0;i<80;++i){const auto key="k"+std::to_string(c)+"_"+std::to_string(i);const auto value="v"+std::to_string(i);send_all(fd.get(),"PUT "+key+" "+value+"\n");if(response(fd.get())!="OK")correct=false;send_all(fd.get(),"GET "+key+"\n");if(response(fd.get())!="VALUE "+value)correct=false;if(i%2U==0U){send_all(fd.get(),"DELETE "+key+"\n");if(response(fd.get())!="OK")correct=false;}}}catch(...){correct=false;}});for(auto& t:clients)t.join();check(correct.load(),"8 concurrent clients complete 640 mutations and reads");server.stop();}{cinderdb::Server recovered{0,path,2,8};recovered.start();check(command(recovered.port(),"GET k3_1\n")=="VALUE v1","server restart replays PUT");check(command(recovered.port(),"GET k3_2\n")=="NOT_FOUND","server restart replays DELETE");recovered.stop();}std::filesystem::remove(path);}
}
int main(){test_protocol();test_fd();test_store_wal();test_server_and_stress();if(failures!=0)return EXIT_FAILURE;std::cout<<"All CinderDB tests passed\n";return EXIT_SUCCESS;}
