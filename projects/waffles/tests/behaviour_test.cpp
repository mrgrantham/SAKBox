// Pins down the behaviour of Waffles code that had no tests of its own, so
// rewrites of it (for readability or to fix clang-tidy findings) can be
// checked to change nothing. Where a value is a fingerprint rather than
// something meaningful, it was recorded from the code before the rewrite.
#include <gtest/gtest.h>

#include <WClasses/GDom.h>
#include <WClasses/GError.h>
#include <WClasses/GHttp.h>
#include <WClasses/GFunction.h>
#include <WClasses/GImage.h>
#include <WClasses/GLearner.h>
#include <WClasses/GMatrix.h>
#include <WClasses/GNeuralNet.h>
#include <WClasses/GRayTrace.h>
#include <WClasses/GTransform.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace GClasses;

// --- GHttpParamParser: which characters a scrubbed value keeps -------------

TEST(HttpParamParserTest, ScrubKeepsOnlySafeCharacters) {
  std::string kept;
  for (char c = ' '; c <= '~'; c++) {
    if (c == '&' || c == '=' || c == '%' || c == '+')
      continue; // parameter syntax, not value characters
    const std::string params = std::string("v=x") + c + "x";
    GHttpParamParser pp(params.c_str());
    const char *v = pp.find("v");
    ASSERT_NE(v, nullptr) << "char " << c;
    ASSERT_EQ(strlen(v), 3u) << "char " << c;
    if (v[1] == c && c != '_') // ('_' is also what the others become)
      kept += c;
    else
      EXPECT_EQ(v[1], '_') << "char " << c;
  }
  EXPECT_EQ(kept, " !#$()*,-./0123456789:<>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                  "abcdefghijklmnopqrstuvwxyz");
}

// --- GHttpClient: what losing the connection means ---------------------------

namespace {
// (status() reports Aborted once the connection is gone, so read the
// outcome onLoseConnection records directly)
struct ClientInState : GHttpClient {
  ClientInState(bool chunked, size_t contentSize) {
    m_bChunked = chunked;
    m_nContentSize = contentSize;
    m_status = Downloading;
  }
  Status outcome() const { return m_status; }
};
GHttpClient::Status statusAfterLosingConnection(bool chunked,
                                                size_t contentSize) {
  ClientInState client(chunked, contentSize);
  client.onLoseConnection();
  return client.outcome();
}
} // namespace

TEST(HttpClientTest, LosingTheConnectionMidBodyIsAnError) {
  // The body's length was known (chunked, or a Content-Length): cut short
  EXPECT_EQ(statusAfterLosingConnection(true, 0), GHttpClient::Error);
  EXPECT_EQ(statusAfterLosingConnection(false, 100), GHttpClient::Error);
  // No length given: the server ends the body by closing the connection
  EXPECT_EQ(statusAfterLosingConnection(false, 0), GHttpClient::Done);
}

// --- GCSVParser: missing and invalid values ----------------------------------

TEST(CsvParserTest, EmptyAndQuestionMarkAreMissing) {
  // Column 0 is numbers, column 1 is categories; rows 2 and 3 leave both
  // out, one way each
  const char *csv = "1.5,red\n2.5,blue\n,\n?,?\n3.5,red\n";
  GCSVParser parser;
  GMatrix m;
  parser.parse(m, csv, strlen(csv));
  ASSERT_EQ(m.rows(), 5u);
  ASSERT_EQ(m.relation().valueCount(0), 0u); // numbers
  ASSERT_EQ(m.relation().valueCount(1), 2u); // red, blue
  EXPECT_EQ(m[0][0], 1.5);
  EXPECT_EQ(m[2][0], UNKNOWN_REAL_VALUE);
  EXPECT_EQ(m[3][0], UNKNOWN_REAL_VALUE);
  EXPECT_EQ(m[4][0], 3.5);
  EXPECT_EQ(m[0][1], m[4][1]); // both red
  EXPECT_NE(m[0][1], m[1][1]);
  EXPECT_EQ(m[2][1], UNKNOWN_DISCRETE_VALUE);
  EXPECT_EQ(m[3][1], UNKNOWN_DISCRETE_VALUE);
}

TEST(CsvParserTest, InvalidNumberInANumberColumnIsMissing) {
  const char *csv = "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n"
                    "17\n18\n19\n20\n21\nabc\n";
  GCSVParser parser;
  parser.setRealAttr(0);
  GMatrix m;
  parser.parse(m, csv, strlen(csv));
  ASSERT_EQ(m.rows(), 22u);
  EXPECT_EQ(m[20][0], 21.0);
  EXPECT_EQ(m[21][0], UNKNOWN_REAL_VALUE);
}

// --- GNominalToCat: how many columns each column becomes ---------------------

TEST(NominalToCatTest, OneHotOnlyBetweenThreeValuesAndTheCap) {
  // A number, then categories with 2, 3, 5, 11 and 12 values (cap 12)
  std::vector<size_t> valueCounts = {0, 2, 3, 5, 11, 12};
  GMixedRelation rel(valueCounts);
  GNominalToCat ntc(12);
  ntc.train(rel);
  // 1 + 1 + 3 + 5 + 11 + 1
  EXPECT_EQ(ntc.after().size(), 22u);

  // A row: number 0.25, then values 1, 2, 4, 10, 7
  GVec in(6), out(ntc.after().size());
  in[0] = 0.25;
  in[1] = 1;
  in[2] = 2;
  in[3] = 4;
  in[4] = 10;
  in[5] = 7;
  ntc.transform(in, out);
  EXPECT_EQ(out[0], 0.25);
  EXPECT_EQ(out[1], 1.0); // two values: one column, 0 or 1
  for (size_t i = 0; i < 3; i++)
    EXPECT_EQ(out[2 + i], i == 2 ? 1.0 : 0.0) << "3-valued, column " << i;
  for (size_t i = 0; i < 5; i++)
    EXPECT_EQ(out[5 + i], i == 4 ? 1.0 : 0.0) << "5-valued, column " << i;
  for (size_t i = 0; i < 11; i++)
    EXPECT_EQ(out[10 + i], i == 10 ? 1.0 : 0.0) << "11-valued, column " << i;
  EXPECT_EQ(ntc.after().valueCount(21), 0u); // 12 values: a single number

  // And back
  GVec back(6);
  ntc.untransform(out, back);
  for (size_t i = 0; i < 5; i++)
    EXPECT_EQ(back[i], in[i]) << "column " << i;
}

// --- GDataPreprocessor::autoTrans: when numbers are rescaled -----------------

namespace {
// Whether autoTrans rescales a single number column holding [lo, hi] for a
// target range of [-1, 1], and if so where lo and hi end up
bool rescales(double lo, double hi, double *pLoOut = nullptr,
              double *pHiOut = nullptr) {
  GMatrix data(2, 1);
  data[0][0] = lo;
  data[1][0] = hi;
  std::unique_ptr<GIncrementalTransform> trans(
      GDataPreprocessor::autoTrans(data, false, false, true, -1.0, 1.0));
  if (!trans)
    return false;
  trans->train(data);
  GVec out(1);
  trans->transform(data[0], out);
  if (pLoOut)
    *pLoOut = out[0];
  trans->transform(data[1], out);
  if (pHiOut)
    *pHiOut = out[0];
  return true;
}
} // namespace

TEST(AutoTransTest, RescalesWhenOutOfRangeOrUsingLittleOfIt) {
  double lo, hi;
  EXPECT_TRUE(rescales(0.0, 5.0, &lo, &hi)); // past the top
  EXPECT_DOUBLE_EQ(lo, -1.0);
  EXPECT_DOUBLE_EQ(hi, 1.0);
  EXPECT_TRUE(rescales(-3.0, 0.5)); // past the bottom
  EXPECT_TRUE(rescales(0.0, 0.1, &lo, &hi)); // under a quarter of the range
  EXPECT_DOUBLE_EQ(lo, -1.0);
  EXPECT_DOUBLE_EQ(hi, 1.0);
  EXPECT_FALSE(rescales(-0.9, 0.9)); // fits and uses most of it
  EXPECT_FALSE(rescales(0.0, 0.6));  // over a quarter: left alone
  EXPECT_FALSE(rescales(0.3, 0.3));  // constant: nothing to spread out
}

// --- GAutoFilter: when it rescales what a learner sees ----------------------

namespace {
// A learner that wants values in [-1, 1] and records the range of the
// features and labels it is actually trained on
struct RangeRecorder : GSupervisedLearner {
  double featureMin = 0, featureMax = 0, labelMin = 0, labelMax = 0;
  bool supportedFeatureRange(double *pMin, double *pMax) override {
    *pMin = -1.0;
    *pMax = 1.0;
    return false;
  }
  bool supportedLabelRange(double *pMin, double *pMax) override {
    *pMin = -1.0;
    *pMax = 1.0;
    return false;
  }
  void trainInner(const GMatrix &features, const GMatrix &labels) override {
    featureMin = features.columnMin(0);
    featureMax = features.columnMax(0);
    labelMin = labels.columnMin(0);
    labelMax = labels.columnMax(0);
  }
  GDomNode *serialize(GDom *) const override { throw Ex("not needed"); }
  void predict(const GVec &, GVec &out) override { out.fill(0.0); }
  void predictDistribution(const GVec &, GPrediction *) override {}
  void clear() override {}
};

// Trains an auto-filtered RangeRecorder on one feature column holding
// [fLo, fHi] and one label column holding [lLo, lHi]
void trainThroughAutoFilter(RangeRecorder *pRec, double fLo, double fHi,
                            double lLo, double lHi) {
  GAutoFilter af(pRec, false);
  GMatrix features(2, 1), labels(2, 1);
  features[0][0] = fLo;
  features[1][0] = fHi;
  labels[0][0] = lLo;
  labels[1][0] = lHi;
  af.train(features, labels);
}
} // namespace

TEST(AutoFilterTest, RescalesValuesUsingLittleOfTheRange) {
  // Features use under a quarter of [-1, 1]: spread out. Labels use most of
  // it: left alone.
  RangeRecorder a;
  trainThroughAutoFilter(&a, 0.0, 0.1, -0.9, 0.9);
  EXPECT_DOUBLE_EQ(a.featureMin, -1.0);
  EXPECT_DOUBLE_EQ(a.featureMax, 1.0);
  EXPECT_DOUBLE_EQ(a.labelMin, -0.9);
  EXPECT_DOUBLE_EQ(a.labelMax, 0.9);

  // And the other way round
  RangeRecorder b;
  trainThroughAutoFilter(&b, -0.9, 0.9, 0.0, 0.1);
  EXPECT_DOUBLE_EQ(b.featureMin, -0.9);
  EXPECT_DOUBLE_EQ(b.featureMax, 0.9);
  EXPECT_DOUBLE_EQ(b.labelMin, -1.0);
  EXPECT_DOUBLE_EQ(b.labelMax, 1.0);

  // Out of range: rescaled into it
  RangeRecorder c;
  trainThroughAutoFilter(&c, 0.0, 5.0, -3.0, 0.5);
  EXPECT_DOUBLE_EQ(c.featureMin, -1.0);
  EXPECT_DOUBLE_EQ(c.featureMax, 1.0);
  EXPECT_DOUBLE_EQ(c.labelMin, -1.0);
  EXPECT_DOUBLE_EQ(c.labelMax, 1.0);
}

// --- GFunctionParser: numbers written without a leading 0 ----------------------

TEST(FunctionParserTest, NumbersMayStartWithAPoint) {
  GFunctionParser mfp;
  mfp.add("f(x)=x*.5+.25");
  GFunction *pFunc = mfp.getFunction("f");
  std::vector<double> params = {3.0};
  EXPECT_DOUBLE_EQ(pFunc->call(params, mfp), 1.75);
}

// --- G3dLetterMaker: letter widths -------------------------------------------

TEST(LetterMakerTest, LetterWidths) {
  GRayTracePhysicalMaterial material; // its mesh builder needs one
  G3dLetterMaker maker(&material);
  const double line = 0.1, space = 0.4; // its defaults
  auto width = [&](char c) { return maker.letterWidth(c) - line; };
  EXPECT_DOUBLE_EQ(width(' '), space);
  EXPECT_DOUBLE_EQ(width('"'), 2 * line);
  EXPECT_DOUBLE_EQ(width('\''), line);
  for (char c : std::string("(),i"))
    EXPECT_DOUBLE_EQ(width(c), 0.1) << c;
  EXPECT_DOUBLE_EQ(width('.'), 0.2);
  for (char c : std::string("jl"))
    EXPECT_DOUBLE_EQ(width(c), 0.225) << c;
  EXPECT_DOUBLE_EQ(width('t'), 0.3);
  for (char c : std::string("+-"))
    EXPECT_DOUBLE_EQ(width(c), 0.4) << c;
  for (char c : std::string("@MWmqw"))
    EXPECT_DOUBLE_EQ(width(c), 0.6) << c;
  for (char c : std::string("aAbBzZ09?"))
    EXPECT_DOUBLE_EQ(width(c), 0.5) << c; // everything else
}

// --- GImage::text: rendered pixels -------------------------------------------

namespace {
uint64_t fingerprint(GImage &image) {
  uint64_t h = 1469598103934665603ull; // FNV-1a
  const unsigned int *p = image.pixels();
  for (size_t i = 0; i < (size_t)image.width() * image.height(); i++) {
    h ^= p[i];
    h *= 1099511628211ull;
  }
  return h;
}
} // namespace

TEST(ImageTextTest, RenderingIsUnchanged) {
  // Every printable character, at a few sizes (fingerprint recorded from
  // the code before the rewrite of GImage::textChar's drawing rules)
  std::string all;
  for (char c = ' '; c <= '~'; c++)
    all += c;
  GImage image;
  image.setSize(1200, 160);
  image.clear(0xff000000);
  image.text(all.c_str(), 0, 0, 1.0f, 0xffffffff);
  image.text(all.c_str(), 0, 40, 1.7f, 0xffff0000);
  image.text(all.c_str(), 0, 90, 2.3f, 0xff00ff00);
  EXPECT_EQ(fingerprint(image), 18229812759536327879ull);
}

// --- GJsonAsADatabase: what a missing field becomes ---------------------------

namespace {
// The database after the commands, and whether one of them threw
std::string jsonAfter(const std::vector<const char *> &cmds,
                      bool *pThrew = nullptr) {
  const char *tmp = getenv("TEST_TMPDIR");
  std::filesystem::path dir =
      std::filesystem::path(tmp ? tmp : "/tmp") / "jaad_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  {
    std::ofstream f(dir / "db.json");
    f << "{}"; // an empty database
  }
  GJsonAsADatabase db((dir.string() + "/").c_str());
  GDom response;
  for (const char *cmd : cmds) {
    try {
      db.apply("db.json", cmd, &response);
    } catch (const std::exception &) {
      if (!pThrew)
        throw;
      *pThrew = true;
    }
  }
  const GDomNode *root = db.apply("db.json", "", &response);
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

// --- GRelation::print: how column types are written --------------------------

TEST(RelationPrintTest, DatesAreWrittenAsReal) {
  GArffRelation rel;
  rel.setName("r");
  std::vector<const char *> values = {"x", "y"};
  rel.addAttribute("n", 0, nullptr);
  rel.addAttribute("c", 2, &values);
  rel.addAttribute("s", (size_t)-1, nullptr); // string
  rel.addAttribute("d", (size_t)-2, nullptr); // date
  std::ostringstream os;
  rel.print(os);
  EXPECT_EQ(os.str(), "@RELATION r\n\n@ATTRIBUTE n real\n@ATTRIBUTE c {x,y}\n"
                      "@ATTRIBUTE s string\n@ATTRIBUTE d real\n\n@DATA\n");
}

// --- GNeuralNetLearner::supportedLabelRange ----------------------------------

TEST(NeuralNetLearnerTest, LabelRangeIsTanhs) {
  GNeuralNetLearner learner;
  double lo = 0, hi = 0;
  EXPECT_FALSE(learner.supportedLabelRange(&lo, &hi));
  EXPECT_EQ(lo, -1.0);
  EXPECT_EQ(hi, 1.0);
  learner.nn().add(new GBlockLinear(3, 2));
  lo = hi = 0;
  EXPECT_FALSE(learner.supportedLabelRange(&lo, &hi));
  EXPECT_EQ(lo, -1.0);
  EXPECT_EQ(hi, 1.0);
}

// --- Ex: the message ----------------------------------------------------------

TEST(ExTest, MessageIsThePartsJoined) {
  EXPECT_STREQ(Ex("a", "b", "c").what(), "abc");
  GExpectException expected; // the other branch of Ex::setMessage
  EXPECT_STREQ(Ex("d", "e").what(), "de");
}
