#include "mongo/util/huglo/huglo.h"

#include "mongo/unittest/unittest.h"

namespace mongo {
namespace {

TEST(Huglo, SayHello) {
    huglo::sayHello();
    ASSERT_TRUE(true);
}

}  // namespace
}  // namespace mongo
