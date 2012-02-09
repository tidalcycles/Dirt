
extern int audio_callback(int samples, float *buffer) {
  int i;
  for (i=0; i < samples; ++i) {
    buffer[0] = 0;
  }
  return(0);
}
