"""Execute the production timing selector with distinct synthetic resource values."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'alarm_system.cpp').read_text()
start = source.index('void AlarmSystemPrivate::updateTargetStateValues()')
end = source.index('\n/*!', start)
method = source[start:end]
suffixes = sorted(set(re.findall(r'\bR(?:Config\w+|InvalidSuffix)\b', method)))
constants = '\n'.join('const char *'+name+'="'+name+'";' for name in suffixes)
harness = '''
#include <cassert>
#include <map>
#include <string>
#define DBG_Assert(x) assert(x)
constexpr int AS_ArmModeDisarmed=0, AS_ArmModeArmedAway=3;
'''+constants+'''
struct Item { int value=0; int toNumber() const { return value; } };
struct Resource { std::map<std::string,Item> items; Item *item(const char *key) { return &items.at(key); } };
struct AlarmSystemPrivate {
    Resource *q; int targetState=0, exitDelay=0, entryDelay=0, triggerDuration=0, armMask=0;
    int targetArmMask[4]={0,2,4,1};
    void updateTargetStateValues();
};
'''+method+'''
int main() {
    Resource q;
    const char *entries[]={RConfigDisarmedEntryDelay,RConfigArmedStayEntryDelay,RConfigArmedNightEntryDelay,RConfigArmedAwayEntryDelay};
    const char *exits[]={RConfigDisarmedExitDelay,RConfigArmedStayExitDelay,RConfigArmedNightExitDelay,RConfigArmedAwayExitDelay};
    const char *durations[]={RInvalidSuffix,RConfigArmedStayTriggerDuration,RConfigArmedNightTriggerDuration,RConfigArmedAwayTriggerDuration};
    for(int mode=0;mode<4;++mode) {
        q.items[entries[mode]].value=10+mode;
        q.items[exits[mode]].value=20+mode;
        if(mode)q.items[durations[mode]].value=30+mode;
    }
    AlarmSystemPrivate state;state.q=&q;
    for(int mode=3;mode>=0;--mode) {
        state.targetState=mode;state.updateTargetStateValues();
        assert(state.entryDelay==10+mode);
        assert(state.exitDelay==20+mode);
        assert(state.triggerDuration==(mode?30+mode:0));
        assert(state.armMask==state.targetArmMask[mode]);
    }
}
'''
with tempfile.TemporaryDirectory() as folder:
    path=Path(folder)/'timing.cpp';path.write_text(harness)
    binary=Path(folder)/'timing'
    subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror',str(path),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('PASS: 4 alarm timing modes')
