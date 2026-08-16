#include "cinderdb/server.hpp"
#include <atomic>
#include <charconv>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
namespace { volatile std::sig_atomic_t stop_requested{0}; void on_signal(int){ stop_requested=1; }
bool parse_size(const char* text,std::size_t& value) { unsigned long long parsed=0; const char* end=text; while(*end!='\0') ++end; const auto [where,error]=std::from_chars(text,end,parsed); if(error!=std::errc{}||where!=end||parsed==0||parsed>std::numeric_limits<std::size_t>::max()) return false; value=static_cast<std::size_t>(parsed); return true; }
void usage(const char* program){ std::cerr<<"Usage: "<<program<<" --port <1-65535> --data <path> --workers <positive> [--queue-capacity <positive>]\n"; }
}
int main(int argc,char* argv[]) { std::size_t port=0,workers=0,capacity=64; std::string path; for(int i=1;i<argc;++i){ std::string option{argv[i]}; if(option=="--help"){usage(argv[0]);return 0;} if(option!="--port"&&option!="--data"&&option!="--workers"&&option!="--queue-capacity"){std::cerr<<"Error: unknown option: "<<option<<'\n';usage(argv[0]);return 2;} if(i+1>=argc){std::cerr<<"Error: missing value for "<<option<<'\n';return 2;} const char* value=argv[++i]; if(option=="--data") path=value; else {std::size_t parsed=0; if(!parse_size(value,parsed)||(option=="--port"&&parsed>65535U)){std::cerr<<"Error: invalid value for "<<option<<'\n';return 2;} if(option=="--port") port=parsed; else if(option=="--workers") workers=parsed; else capacity=parsed;} } if(port==0||workers==0||path.empty()){std::cerr<<"Error: --port, --data, and --workers are required\n";usage(argv[0]);return 2;} std::signal(SIGINT,on_signal); std::signal(SIGTERM,on_signal); try { cinderdb::Server server{static_cast<unsigned short>(port),path,workers,capacity}; server.start(); std::cout<<"CinderDB listening on 127.0.0.1:"<<server.port()<<'\n'; while(stop_requested==0) std::this_thread::sleep_for(std::chrono::milliseconds{50}); server.stop(); return 0; } catch(const std::exception& error){std::cerr<<"Error: "<<error.what()<<'\n';return 1;} }
