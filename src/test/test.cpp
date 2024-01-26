#include <protobuf-cpp/protobuf-cpp.hpp>
#include <catch2/catch_all.hpp>
#include "simple.pb.h"
#include "simple.hpp"
using std::string;


void println(auto&&... args) {
  (std::cout << ... << args) << std::endl;
}

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

    // encode back again
    pbcpp::encoder::to_string(msg2);
  }

  SECTION("to_string") {
    tmp::test::SimpleMessage msg;
    msg.name = "Michael";
    msg.num = 1922211;
    msg.nums = {1,2,3,4};
    REQUIRE( std::to_string(msg) == "{name:Michael,num:1922211,nums:[1,2,3,4]}" );
  }

  SECTION("compare") {
    tmp::test::SimpleMessage m1, m2;
    m1.num = 12;

    REQUIRE( m1 > m2 );
    REQUIRE( m1 != m2 );

    m2.num = 12;
    REQUIRE( m1 == m2 );

    m1.name = "bob";
    REQUIRE( m1 > m2 );

    m2.name = m1.name;
    m1.nums = {3,2,1};
    m2.nums = {3,1,2,4};
    REQUIRE( m1 < m2 );
    m1.nums.push_back(4);
    REQUIRE( m1 > m2 );
  }

  SECTION("hash") {
    tmp::test::SimpleMessage m1;
    std::unordered_set<tmp::test::SimpleMessage> set;

    set.insert(m1);
    m1.name = "bob";
    set.insert(m1);
    REQUIRE( set.size() == 2 );
    m1.nums = {1,2,3,4};
    set.insert(m1);
    REQUIRE( set.size() == 3 );

    tmp::test::SimpleMessage m2;
    m2.name = "bob";
    m2.nums = {1,2,3,4};
    REQUIRE( (set.find(m2) != set.end()) );
  }
}

