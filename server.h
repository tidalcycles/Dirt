extern int server_init(const char *osc_port);
extern void osc_send_pitch(double starttime, unsigned int chunk, 
			   float pitch, float flux, float centroid);
extern void osc_send_play(double when, int lowchunk, float pitch, float flux, float centroid);
