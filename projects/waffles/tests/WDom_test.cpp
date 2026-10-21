// WDom's JSON database, WJsonAsADatabase.
#include <gtest/gtest.h>

#include <WClasses/WDecisionTree.h>
#include <WClasses/WDistance.h>
#include <WClasses/WDom.h>
#include <WClasses/WLearner.h>
#include <WClasses/WMatrix.h>
#include <WClasses/WNeuralNet.h>
#include <WClasses/WRand.h>
#include <WClasses/WRecommender.h>
#include <WClasses/WTransform.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace WClasses;

namespace {
// The database after the commands, and whether one of them threw
std::string jsonAfter(const std::vector<const char *> &cmds, bool *pThrew = nullptr) {
  const char *tmp = getenv("TEST_TMPDIR");
  std::filesystem::path dir = std::filesystem::path(tmp ? tmp : "/tmp") / "jaad_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  {
    std::ofstream f(dir / "db.json");
    f << "{}"; // an empty database
  }
  WJsonAsADatabase db((dir.string() + "/").c_str());
  WDom response;
  for (const char *cmd : cmds) {
    try {
      db.apply("db.json", cmd, &response);
    } catch (const std::exception &) {
      if (!pThrew)
        throw;
      *pThrew = true;
    }
  }
  const WDomNode *root = db.apply("db.json", "", &response);
  std::ostringstream os;
  if (root)
    root->writeJson(os);
  return os.str();
}
} // namespace

// (Through "+=", which uses findNode; "=" goes through findLValue)
TEST(JsonAsADatabaseTest, MissingFieldsAreCreatedByWhatComesNext) {
  // Nothing after the field: "+=" makes it a list to append to
  EXPECT_EQ(jsonAfter({".c += 2"}), "{\"c\":[2]}");
  // '.' after it: an object
  EXPECT_EQ(jsonAfter({".e.f += 4"}), "{\"e\":{\"f\":[4]}}");
  // '[' after it: a list. (Indexing the new, empty list then fails, so
  // this leaves an empty list behind and throws.)
  bool threw = false;
  EXPECT_EQ(jsonAfter({".h[0] += 6"}, &threw), "{\"h\":[]}");
  EXPECT_TRUE(threw);
}

// Assigning ("=") a field that didn't exist yet went wrong in findLValue:
// with more path after it (".a.b = 1"), it called add on the null pointer
// it had just got back, crashing; as the last field (".a = 1") it returned
// that null, which apply took to mean "no document" and replaced the whole
// database with the value. Indexing one past the end of a list with more
// path after it (".l[1].x = 2") appended a node, then looked up the index
// in that new node instead of the list.
TEST(JsonAsADatabaseTest, AssigningCreatesWhatsMissing) {
  EXPECT_EQ(jsonAfter({".a = 1"}), "{\"a\":1}");
  EXPECT_EQ(jsonAfter({".a = 1", ".b = 2"}), "{\"a\":1,\"b\":2}");
  EXPECT_EQ(jsonAfter({".a.b = 1"}), "{\"a\":{\"b\":1}}");
  EXPECT_EQ(jsonAfter({".a[0] = 5"}), "{\"a\":[5]}");
  EXPECT_EQ(jsonAfter({".l += 1", ".l[1].x = 2"}), "{\"l\":[1,{\"x\":2}]}");
  // Existing fields and elements are still replaced
  EXPECT_EQ(jsonAfter({".a = 1", ".a = 3"}), "{\"a\":3}");
  EXPECT_EQ(jsonAfter({".l += 1", ".l[0] = 9"}), "{\"l\":[9]}");
}

// JSON numbers were read as integers unless they had a '.', and the integer
// reader stopped at the 'e': 1e5 loaded as 1 and 2E-3 as 2.
TEST(JsonNumberTest, ExponentsAndInvalidNumbers) {
  WDom doc;
  const char *json = "[1e5, 2E-3, 7, 1.5, -3, 12345678901234567890]";
  doc.parseJson(json, strlen(json));
  const WDomNode *list = doc.root();
  ASSERT_EQ(list->size(), 6u);
  EXPECT_DOUBLE_EQ(list->get(size_t(0))->asDouble(), 100000.0);
  EXPECT_DOUBLE_EQ(list->get(size_t(1))->asDouble(), 0.002);
  EXPECT_EQ(list->get(size_t(2))->asInt(), 7);
  EXPECT_DOUBLE_EQ(list->get(size_t(3))->asDouble(), 1.5);
  EXPECT_EQ(list->get(size_t(4))->asInt(), -3);
  // Too big for an integer: read as a decimal instead
  EXPECT_DOUBLE_EQ(list->get(size_t(5))->asDouble(), 12345678901234567890.0);
  for (const char *bad : {"[1-2]", "[+5]", "[1e]", "[-]"}) {
    WDom d;
    EXPECT_THROW(d.parseJson(bad, strlen(bad)), std::exception) << bad;
  }
}

// Numbers saved as JSON text read back exactly. They used to be written
// with 6 decimal places, so 0.1234567891 came back as 0.123457 and 1e-9
// as 0.
namespace {
const std::vector<double> awkward = {0.1,     1.0 / 3, 1e-9, 1e300, -0.0, 123456.789,
                                     -2.5e-7, 5e-324,  1.0,  -42.0, 2e20, 0.0};

// `value` written by `write` into a document's text, parsed back into `doc`
WDomNode *throughText(double value, WDom &doc, bool pretty) {
  WDom saved;
  saved.setRoot(saved.newObj());
  saved.root()->add(&saved, "v", value);
  std::ostringstream text;
  if (pretty)
    saved.writeJsonPretty(text);
  else
    saved.writeJson(text);
  doc.parseJson(text.str().c_str(), text.str().size());
  return doc.root()->get("v");
}
} // namespace

TEST(JsonNumberTest, DoublesReadBackExactly) {
  for (bool pretty : {false, true})
    for (double value : awkward) {
      WDom doc;
      const WDomNode *node = throughText(value, doc, pretty);
      EXPECT_EQ(node->type(), WDomNode::type_double) << value; // even whole numbers
      EXPECT_EQ(node->asDouble(), value) << value << (pretty ? " (pretty)" : "");
      EXPECT_EQ(std::signbit(node->asDouble()), std::signbit(value)) << value;
    }
}

// The shortest text that reads back exactly, with a decimal point kept on
// whole numbers
TEST(JsonNumberTest, DoublesAreWrittenShort) {
  const std::vector<std::pair<double, std::string>> cases = {
      {0.1, "0.1"}, {1.0, "1.0"}, {-42.0, "-42.0"}, {1e-9, "1e-09"}, {2e20, "2e+20"}};
  for (const auto &[value, text] : cases) {
    WDom doc;
    doc.setRoot(doc.newDouble(value));
    std::ostringstream out;
    doc.writeJson(out);
    EXPECT_EQ(out.str(), text);
  }
}

// A trained network saved as JSON text predicts exactly what it did
TEST(JsonNumberTest, SavedNeuralNetPredictsTheSame) {
  WRand rand(0);
  WNeuralNet nn;
  nn.add(new WBlockLinear(2, 3));
  nn.add(new WBlockTanh(3));
  nn.add(new WBlockLinear(3, 1));
  nn.init(rand);
  WDom saved;
  std::ostringstream text;
  nn.serialize(&saved)->writeJson(text);
  WDom doc;
  doc.parseJson(text.str().c_str(), text.str().size());
  WNeuralNet loaded(doc.root(), rand);
  const WVec input({0.5, -1.0});
  WVec expected;
  expected.copy(nn.forwardProp(input));
  EXPECT_EQ(loaded.forwardProp(input)[0], expected[0]);
}

// Waffles' classes were renamed from G... to W..., and files saved before
// then name them the old way ("class": "GDecisionTree"). Each loader still
// loads them: these save an object, rewrite its class names as an old file
// has them, and load that.
namespace {
// `node` as a file saved before the rename has it: saved as JSON text, its
// class names rewritten the old way, and parsed back into `doc`
WDomNode *savedWithOldNames(const WDomNode *node, WDom &doc) {
  std::ostringstream json;
  node->writeJson(json);
  const std::string old = std::regex_replace(
      json.str(), std::regex(R"re("(class|type)":"W([A-Z0-9]))re"), R"re("$1":"G$2)re");
  EXPECT_NE(old, json.str()) << "no class names were rewritten";
  EXPECT_EQ(old.find(R"("class":"W)"), std::string::npos) << old;
  doc.parseJson(old.c_str(), old.size());
  return doc.root();
}
} // namespace

TEST(OldClassNameTest, IsReadAsTheNewName) {
  WDom doc;
  const char *json = R"({"a":"GMatrix","b":"WMatrix","c":"G3DVector","d":"Gaussian","e":"rbf"})";
  doc.parseJson(json, strlen(json));
  EXPECT_EQ(doc.root()->getClassName("a"), "WMatrix");
  EXPECT_EQ(doc.root()->getClassName("b"), "WMatrix");
  EXPECT_EQ(doc.root()->getClassName("c"), "W3DVector");
  EXPECT_EQ(doc.root()->getClassName("d"), "Gaussian"); // not a prefix
  EXPECT_EQ(doc.root()->getClassName("e"), "rbf");
}

// A random forest, which saves the class names of its trees too
TEST(OldClassNameTest, LearnerLoads) {
  WMatrix features(0, 1), labels(0, 1);
  for (int i = 0; i < 20; i++) {
    features.newRow()[0] = i;
    labels.newRow()[0] = i < 10 ? 0.0 : 1.0;
  }
  WRandomForest forest(3);
  forest.rand().setSeed(0);
  forest.train(features, labels);
  WDom saved, old;
  WLearnerLoader loader;
  std::unique_ptr<WSupervisedLearner> loaded(
      loader.loadLearner(savedWithOldNames(forest.serialize(&saved), old)));
  ASSERT_NE(loaded, nullptr);
  WVec expected(1), got(1);
  for (double x : {2.0, 15.0}) {
    forest.predict(WVec({x}), expected);
    loaded->predict(WVec({x}), got);
    EXPECT_EQ(got[0], expected[0]) << x;
  }
}

TEST(OldClassNameTest, TransformLoads) {
  WMatrix data(0, 2);
  WVec &a = data.newRow();
  a[0] = 1;
  a[1] = 10;
  WVec &b = data.newRow();
  b[0] = 3;
  b[1] = 30;
  WNormalize normalize;
  normalize.train(data);
  WDom saved, old;
  WLearnerLoader loader;
  std::unique_ptr<WIncrementalTransform> loaded(
      loader.loadIncrementalTransform(savedWithOldNames(normalize.serialize(&saved), old)));
  ASSERT_NE(loaded, nullptr);
  WVec expected(2), got(2); // transform writes into vectors of the right size
  normalize.transform(WVec({2.0, 20.0}), expected);
  loaded->transform(WVec({2.0, 20.0}), got);
  ASSERT_EQ(got.size(), expected.size());
  for (size_t i = 0; i < got.size(); i++)
    EXPECT_EQ(got[i], expected[i]) << i;
}

TEST(OldClassNameTest, RecommenderLoads) {
  WMatrix ratings(0, 3);
  for (size_t user = 0; user < 3; user++)
    for (size_t item = 0; item < 3; item++) {
      WVec &row = ratings.newRow();
      row[0] = (double)user;
      row[1] = (double)item;
      row[2] = (double)(user + 2 * item);
    }
  WBaselineRecommender baseline;
  baseline.train(ratings);
  WDom saved, old;
  WLearnerLoader loader;
  std::unique_ptr<WCollaborativeFilter> loaded(
      loader.loadCollaborativeFilter(savedWithOldNames(baseline.serialize(&saved), old)));
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->predict(1, 2), baseline.predict(1, 2));
}

TEST(OldClassNameTest, DistanceMetricLoads) {
  WMatrix data(0, 2);
  WRowDistance metric;
  metric.init(&data.relation(), false);
  WDom saved, old;
  std::unique_ptr<WDistanceMetric> loaded(
      WDistanceMetric::deserialize(savedWithOldNames(metric.serialize(&saved), old)));
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->squaredDistance(WVec({0.0, 0.0}), WVec({3.0, 4.0})), 25.0);
}

TEST(OldClassNameTest, SparseSimilarityLoads) {
  WCosineSimilarity similarity;
  WDom saved, old;
  std::unique_ptr<WSparseSimilarity> loaded(
      WSparseSimilarity::deserialize(savedWithOldNames(similarity.serialize(&saved), old)));
  ASSERT_NE(loaded, nullptr);
  EXPECT_DOUBLE_EQ(loaded->similarity(WVec({1.0, 0.0}), WVec({1.0, 1.0})),
                   similarity.similarity(WVec({1.0, 0.0}), WVec({1.0, 1.0})));
}

// A neural network's blocks give their class as "type"
TEST(OldClassNameTest, NeuralNetBlocksLoad) {
  WRand rand(0);
  WNeuralNet nn;
  nn.add(new WBlockLinear(2, 3));
  nn.add(new WBlockTanh(3));
  nn.init(rand);
  WDom saved, old;
  WNeuralNet loaded(savedWithOldNames(nn.serialize(&saved), old), rand);
  const WVec input({0.5, -1.0});
  WVec expected;
  expected.copy(nn.forwardProp(input));
  const WVec &got = loaded.forwardProp(input);
  ASSERT_EQ(got.size(), expected.size());
  for (size_t i = 0; i < got.size(); i++)
    EXPECT_EQ(got[i], expected[i]) << i;
}
