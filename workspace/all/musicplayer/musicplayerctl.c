#include "music_service_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_snapshot(const MusicSnapshotWire* s) {
	printf("source=%d source_state=%d state=%d file=%s title=%s artist=%s position_ms=%d duration_ms=%d queue=%d/%d eof=%d album=%s artwork=%s\n",
		   s->source, s->source_state, s->state, s->current_file, s->title, s->artist,
		   s->position_ms, s->duration_ms, s->queue_index, s->queue_count, s->stream_eof,
		   s->album, s->artwork_path);
}

int main(int argc, char** argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: musicplayerctl {snapshot|load PATH|radio URL|podcast FEED_URL EPISODE_GUID|select N|play|pause|stop|toggle|next|previous|seek MS|repeat 0|1|shuffle|volume 0-20|sleep|wake|shutdown}\n");
		return 2;
	}
	int playing_query = 0;
	uint16_t command = 0;
	size_t payload_capacity = sizeof(MusicPodcastLoadRequest);
	if (sizeof(MusicLoadRequest) > payload_capacity)
		payload_capacity = sizeof(MusicLoadRequest);
	if (sizeof(MusicIntRequest) > payload_capacity)
		payload_capacity = sizeof(MusicIntRequest);
	unsigned char payload[payload_capacity];
	size_t payload_length = 0;
	memset(payload, 0, sizeof(payload));
	if (!strcmp(argv[1], "snapshot"))
		command = MUSIC_CMD_SNAPSHOT;
	else if (!strcmp(argv[1], "playing")) {
		// 退出码查询：正在播放/连接中返回 0，否则 1（供睡眠链判断是否需要静音）
		command = MUSIC_CMD_SNAPSHOT;
		playing_query = 1;
	}
	else if (!strcmp(argv[1], "load") && argc == 3) {
		command = MUSIC_CMD_LOAD;
		strncpy(((MusicLoadRequest*)payload)->path, argv[2], MUSIC_SERVICE_MAX_PATH - 1);
		payload_length = sizeof(MusicLoadRequest);
	} else if (!strcmp(argv[1], "radio") && argc == 3) {
		command = MUSIC_CMD_RADIO_LOAD;
		strncpy(((MusicRadioLoadRequest*)payload)->url, argv[2], MUSIC_SERVICE_MAX_PATH - 1);
		payload_length = sizeof(MusicRadioLoadRequest);
	} else if (!strcmp(argv[1], "podcast") && argc == 4) {
		command = MUSIC_CMD_PODCAST_LOAD;
		strncpy(((MusicPodcastLoadRequest*)payload)->feed_url, argv[2], MUSIC_SERVICE_MAX_PATH - 1);
		strncpy(((MusicPodcastLoadRequest*)payload)->episode_guid, argv[3], sizeof(((MusicPodcastLoadRequest*)payload)->episode_guid) - 1);
		payload_length = sizeof(MusicPodcastLoadRequest);
	} else if (!strcmp(argv[1], "select") && argc == 3)
		command = MUSIC_CMD_SELECT;
	else if (!strcmp(argv[1], "play"))
		command = MUSIC_CMD_PLAY;
	else if (!strcmp(argv[1], "pause"))
		command = MUSIC_CMD_PAUSE;
	else if (!strcmp(argv[1], "stop"))
		command = MUSIC_CMD_STOP;
	else if (!strcmp(argv[1], "toggle"))
		command = MUSIC_CMD_TOGGLE;
	else if (!strcmp(argv[1], "next"))
		command = MUSIC_CMD_NEXT;
	else if (!strcmp(argv[1], "previous"))
		command = MUSIC_CMD_PREVIOUS;
	else if (!strcmp(argv[1], "seek") && argc == 3)
		command = MUSIC_CMD_SEEK;
	else if (!strcmp(argv[1], "repeat") && argc == 3)
		command = MUSIC_CMD_REPEAT;
	else if (!strcmp(argv[1], "shuffle"))
		command = MUSIC_CMD_SHUFFLE;
	else if (!strcmp(argv[1], "sleep"))
		command = MUSIC_CMD_SLEEP;
	else if (!strcmp(argv[1], "wake"))
		command = MUSIC_CMD_WAKE;
	else if (!strcmp(argv[1], "volume") && argc == 3)
		command = MUSIC_CMD_SET_VOLUME;
	else if (!strcmp(argv[1], "shutdown"))
		command = MUSIC_CMD_SHUTDOWN;
	else {
		fprintf(stderr, "invalid command\n");
		return 2;
	}
	if (command == MUSIC_CMD_SELECT || command == MUSIC_CMD_SEEK || command == MUSIC_CMD_REPEAT ||
		command == MUSIC_CMD_SET_VOLUME) {
		if (argc != 3)
			return 2;
		((MusicIntRequest*)payload)->value = (int32_t)strtol(argv[2], NULL, 10);
		payload_length = sizeof(MusicIntRequest);
	}
	int fd = MusicService_connect(NULL, 1000);
	if (fd < 0) {
		perror("musicplayerd");
		return 1;
	}
	MusicResponseWire response;
	int status = MusicService_request(fd, command, payload, payload_length, &response, 5000);
	MusicService_disconnect(fd);
	if (status < 0) {
		if (playing_query)
			return 1; // daemon 不在（未启动）＝无播放
		fprintf(stderr, "%s\n", response.error[0] ? response.error : "request failed");
		return 1;
	}
	if (playing_query) {
		const MusicSnapshotWire* s = &response.snapshot;
		int playing = s->state == MUSIC_STATE_PLAYING ||
					  s->source_state == MUSIC_RADIO_CONNECTING ||
					  s->source_state == MUSIC_RADIO_BUFFERING ||
					  s->source_state == MUSIC_RADIO_PLAYING;
		return playing ? 0 : 1;
	}
	print_snapshot(&response.snapshot);
	return 0;
}
