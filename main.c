#include "ledwand.h"
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#define MUTE_FLAG (1 << 0)
#define VIS_FLAG  (1 << 3)

static GMainLoop *loop;
static Ledwand ledwand;

static GstVideoInfo video_info;

/*
Callback for new frames
*/
static GstFlowReturn on_new_buffer (GstAppSink* app_sink, gpointer user_data){
    //GstMapInfo map;
    GstVideoFrame frame;

    GstSample *sample = gst_app_sink_pull_sample(app_sink);
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    //gst_buffer_map(buffer, &map, GST_MAP_READ);
    //ledwand_draw_image(&ledwand, map.data, map.size);
    //gst_buffer_unmap(buffer, &map);
    gst_video_frame_map(&frame, &video_info, buffer, GST_MAP_READ);
    ledwand_draw_image(&ledwand, frame.map[0].data, frame.map[0].size/1.5); // devide by 1.5 becaus i420 has 12bits per pixel

    gst_video_frame_unmap(&frame);
    //gst_buffer_unmap(buffer);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

/*
boilerplate event handing code
*/
static gboolean playbin_bus_callback (GstBus *bus,GstMessage *message, gpointer data) {
  //g_print ("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR: {
      GError *err;
      gchar *debug;
      gst_message_parse_error (message, &err, &debug);
      g_print ("Error: %s\n", err->message);
      ledwand_clear(&ledwand);
      ledwand_send(&ledwand, ASCII, 0, 0, strnlen(err->message, 255), 1, (uint8_t*)err->message, strnlen(err->message, 255));
      g_error_free (err);
      g_free (debug);

      g_main_loop_quit (loop);
      break;
    }

    case GST_MESSAGE_BUFFERING: {
        int percent = 0;
        gst_message_parse_buffering (message, &percent);
        g_print ("Buffering (%u percent done)", percent);
        break;
    }

    case GST_MESSAGE_EOS:
      /* end-of-stream */
      g_main_loop_quit (loop);
      break;

    default:
      /* unhandled message */
      break;
  }

  return TRUE;
}

void print_usage(char **argv){
    printf("usage: %s [-a] filename_or_URL\n\t-a\tenable audio\n", argv[0]);
    exit(-1);
}

int main(int argc, char **argv)
{
    if(argc <= 1){
        print_usage(argv);
    }

    if(ledwand_init(&ledwand)){
        perror("FEHLER beim init\n");
        return -1;
    }
    char c;
    char movie[PATH_MAX], absmovie[PATH_MAX];
    char *text = "playing";
    char *text_end = "TEH END";
    int len_end = strnlen(text_end, 255);
    int len = strnlen(text, 255);
    int flags = 0;
    int opt_flags = 0;

    gst_init(&argc, &argv);


    GstElement *playbin = gst_element_factory_make("playbin", "playbin");
    GstAppSink *ledsink = GST_APP_SINK(gst_element_factory_make("appsink", "ledsink"));
    GstCaps *caps = gst_caps_from_string("video/x-raw, format=I420, width=448, height=240");
    if(!gst_video_info_from_caps(&video_info, caps)){
        perror("Error while parsing video info!\n");
        return -1;
    }
    gst_app_sink_set_caps(ledsink, caps);

    GstBus *omnibus = gst_pipeline_get_bus(GST_PIPELINE(playbin));

    if(!playbin || !ledsink || !omnibus){
        perror("Error while creating elements\n");
        return -1;
    }

    gst_bus_add_watch (omnibus, playbin_bus_callback, NULL);
    gst_object_unref(GST_OBJECT(omnibus));

    while (-1 != (c = getopt(argc, argv, "a")))
	switch (c) {
	    case 'a':{
            opt_flags |= MUTE_FLAG;
            break;
	    }

    default:
		print_usage(argv);
	}

    if (optind >= argc) {
        fprintf(stderr, "must specify filename or URL parameter!\n");
        return 1;
    }

    if (strstr(argv[optind], "://"))
        snprintf(movie, sizeof(movie), "%s", argv[optind]);
    else {

        if (!realpath(argv[optind], absmovie)) {
            fprintf(stderr, "file '%s' not found!\n", argv[optind]);
            return 1;
        } else
            snprintf(movie, sizeof(movie), "file://%s", absmovie);
    }

    g_object_set(playbin, "video-sink", ledsink, NULL);

    gst_object_unref(GST_OBJECT(ledsink));

    GstAppSinkCallbacks ledsink_callbacks;

    ledsink_callbacks.new_sample = on_new_buffer;
    gst_app_sink_set_callbacks(ledsink, &ledsink_callbacks, NULL, NULL);

    ledwand_clear(&ledwand);
    ledwand_send(&ledwand, ASCII, 0, 0, len, 1, (uint8_t*)text, len);

    g_object_get (playbin, "flags", &flags, NULL);
    if (opt_flags & MUTE_FLAG){
        flags = 0x01;
    }
    else{
        flags |= VIS_FLAG;
    }
    g_object_set(playbin, "flags", flags , NULL);

    g_object_set(playbin, "uri", movie, NULL);
    loop = g_main_loop_new(NULL, FALSE);
    g_print("playing!\n");
    gst_element_set_state (playbin, GST_STATE_PLAYING);
    g_main_loop_run(loop);

    gst_element_set_state(playbin, GST_STATE_NULL);
    gst_object_unref(playbin);

    //ledwand_clear(&ledwand);
    ledwand_send(&ledwand, ASCII, 25, 9, len, 1, (uint8_t*)text_end, len_end);


    close(ledwand.s_sock);
    //gst_object_unref(GST_OBJECT(omnibus));
    //gst_object_unref(GST_OBJECT(playbin));
    //gst_object_unref(GST_OBJECT(loop));
    return 0;
}

