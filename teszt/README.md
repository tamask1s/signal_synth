# signal_synth tests

Build and run from the repository root:

```sh
g++ -std=c++11 -Wall -Wextra -Wpedantic \
    teszt/signal_synth_test.cpp src/signal_synth.cpp \
    -o /tmp/signal_synth_test
/tmp/signal_synth_test
```

To intentionally refresh the golden summaries:

```sh
/tmp/signal_synth_test --record
```

The test profiles use parameters from the DataBrowser SigForge scripts:

- `sim_ecg_001.sim_par` and `sim_ecg_001.gen_par`
- `045_ECG_Generator.txt`
- `036_generatemodulatedsine.txt`
