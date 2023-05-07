#include "embedded_python.h"
#include "CppUTest/TestHarness.h"

#include <filesystem>
#include <fstream>

static const char MOCK_PYADQ[] = R"(
import ctypes as ct

class ADQ:
    """ Mock of pyadq.ADQ for testing purposes. """
    def __init__(
        self, ADQAPI: ct.CDLL, adq_cu: ct.c_void_p, adq_num: int
    ):
        self._ADQAPI = ADQAPI
        self._adq_cu = adq_cu
        self._adq_num = adq_num
        print("Initialized mockup pyadq.ADQ object.")
)";

static const char WITH_MAIN[] = R"(
def main(adq):
    print(f"Called main() with '{adq}'.")
)";

static const char WITHOUT_MAIN[] = R"(
def definitely_not_main():
    print("No main as far as the eye can see.")
)";

TEST_GROUP(Python)
{
    const std::filesystem::path PYADQ_PATH = std::filesystem::current_path() / "pyadq.py";
    const std::filesystem::path WITH_MAIN_PATH = std::filesystem::current_path() / "with_main.py";
    const std::filesystem::path WITHOUT_MAIN_PATH = std::filesystem::current_path() / "without_main.py";

    void setup()
    {
        std::ofstream ofs{PYADQ_PATH, std::ios::out};
        ofs << MOCK_PYADQ;
        ofs.close();

        ofs = std::ofstream{WITH_MAIN_PATH, std::ios::out};
        ofs << WITH_MAIN;
        ofs.close();

        ofs = std::ofstream{WITHOUT_MAIN_PATH, std::ios::out};
        ofs << WITHOUT_MAIN;
        ofs.close();
    }

    void teardown()
    {
        std::filesystem::remove(PYADQ_PATH);
        std::filesystem::remove(WITH_MAIN_PATH);
        std::filesystem::remove(WITHOUT_MAIN_PATH);
    }
};

TEST(Python, CheckAndCall)
{
    LONGS_EQUAL(1, EmbeddedPython::IsInitialized());

    /* Throws on error. */
    EmbeddedPython::AddToPath(std::filesystem::current_path().string());

    LONGS_EQUAL(0, EmbeddedPython::HasMain("without_main.py"));
    LONGS_EQUAL(1, EmbeddedPython::HasMain("with_main.py"));

    /* Throws on error. */
    int dummy = 1024;
    EmbeddedPython::CallMain("with_main", &dummy, 10);
}
