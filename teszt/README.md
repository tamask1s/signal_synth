# signal_synth tests

CTest exposes these stable verification suite IDs:

- `TEST-SYNTH-001`
- `TEST-ECG-MODEL-001`
- `TEST-ECG-PHANTOM-001`
- `TEST-ECG-MORPH-001`
- `TEST-ECG-SCENARIO-001`
- `TEST-ECG-INFARCTION-001`
- `TEST-ECG-JSON-001`
- `TEST-ECG-EXPORT-001`
- `TEST-PPG-001`
- `TEST-CLI-001`
- `TEST-BUILD-001`

GitHub Actions procedure `CI-VER-001` executes all suites on Linux and
Windows. See `doc/synsigra_architecture_docs/17_TRACEABILITY_SOP.md` for the
difference between a test procedure, an execution result, and retained
verification evidence.

The controlled build and verification path from the repository root is:

```sh
cmake -H. -Bbuild -DSIGNAL_SYNTH_BUILD_TESTS=ON
cmake --build build
cd build
ctest --output-on-failure
```

The following direct compiler commands remain useful for focused legacy
debugging:

```sh
g++ -std=c++11 -Wall -Wextra -Wpedantic \
    teszt/signal_synth_test.cpp src/signal_synth.cpp \
    -o /tmp/signal_synth_test
/tmp/signal_synth_test

g++ -std=c++11 -Wall -Wextra -Wpedantic \
    teszt/ecg_model_test.cpp src/ecg_model.cpp \
    -o /tmp/ecg_model_test
/tmp/ecg_model_test

g++ -std=c++11 -Wall -Wextra -Wpedantic \
    teszt/clinical_ecg_test.cpp src/clinical_ecg.cpp \
    -o /tmp/clinical_ecg_test
/tmp/clinical_ecg_test

g++ -std=c++11 -Wall -Wextra -Wpedantic \
    teszt/ecg_morphology_test.cpp src/ecg_morphology.cpp src/clinical_ecg.cpp \
    -o /tmp/ecg_morphology_test
/tmp/ecg_morphology_test

g++ -std=c++11 -Wall -Wextra -Wpedantic \
    teszt/ecg_scenario_test.cpp src/ecg_scenario.cpp \
    src/ecg_morphology.cpp src/clinical_ecg.cpp \
    -o /tmp/ecg_scenario_test
/tmp/ecg_scenario_test
```

To intentionally refresh the golden summaries:

```sh
/tmp/signal_synth_test --record
```

The test profiles use parameters from the DataBrowser SigForge scripts:

- `sim_ecg_001.sim_par` and `sim_ecg_001.gen_par`
- `045_ECG_Generator.txt`
- `036_generatemodulatedsine.txt`
