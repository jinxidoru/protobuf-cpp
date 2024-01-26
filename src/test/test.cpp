#include <protobuf-cpp.hpp>
#include <catch2/catch_all.hpp>
#include "simple.pb.h"
#include "simple.hpp"
using std::string;



auto get_repeated(auto* coll) {
  return std::vector(coll->begin(), coll->end());
}

auto orig_serialize(auto& msg) {
  std::string data;
  msg.SerializeToString(&data);
  return std::move(data);
}


TEST_CASE("simple message") {

  SECTION("encoding") {
    test::SimpleMessage msg;
    msg.set_name("bob");
    msg.set_num(123990320);
    auto data = orig_serialize(msg);

    // verify that we get the same value when decoding to pbcpp
    tmp::test::SimpleMessage msg2;
    pbcpp::decoder::from_string(data,msg2);
    REQUIRE( msg2.name == msg.name() );
    REQUIRE( msg2.num == msg.num() );
    REQUIRE( msg2.nums == get_repeated(msg.mutable_nums()) );
  }

  SECTION("to_string") {
    tmp::test::SimpleMessage msg;
    msg.name = "Michael";
    msg.num = 1922211;
    msg.nums = {1,2,3,4};
    REQUIRE( std::to_string(msg) == "{name:Michael,num:1922211,nums:[1,2,3,4]}" );
  }
}

