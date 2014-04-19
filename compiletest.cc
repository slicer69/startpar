#include <cstring>

int testfunc(void)
{
  struct {
    char rID[4];
    char wID[4];
    char fId[4];
  } wav;

  std::strncpy(wav.rID, "RIFF", 4);
  std::strncpy(wav.wID, "WAVE", 4);
  std::strncpy(wav.fId, "fmt ", 4);

  std::memcpy(wav.rID, "RIFF", 4);
  std::memcpy(wav.wID, "WAVE", 4);
  std::memcpy(wav.fId, "fmt ", 4);

  return 0;
}
