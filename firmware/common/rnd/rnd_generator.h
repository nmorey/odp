#ifndef RND_GENERATOR__H
#define RND_GENERATOR__H

void
odp_rnd_gen_init();

int
odp_rnd_gen_get(char *buf, int len);

odp_rpc_cmd_ack_t
rnd_send_buffer(unsigned remoteClus, odp_rpc_t * msg);

#endif /* RND_GENERATOR__H */
