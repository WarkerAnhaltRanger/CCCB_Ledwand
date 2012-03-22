#include "ledwand.h"
#include "embedded_audio.h"
#include <stdio.h>
#include <unistd.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#define MUTE_FLAG (1 << 0)
#define EMBD_FLAG (1 << 1)

static GMainLoop *loop;
static Ledwand ledwand;
static Em_Audio em_audio;

static GstAppSinkCallbacks ledsink_callbacks;

static GstFlowReturn on_new_buffer (GstAppSink* app_sink, gpointer user_data){
    //GstAppSink *app_sink = (GstAppSink*) object;
    GstBuffer *buffer = gst_app_sink_pull_buffer(app_sink);
    ledwand_draw_image(&ledwand, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));
    return GST_FLOW_OK;
}

static void on_new_audio_buffer(GstElement *obj, gpointer user_data){
    GstAppSink *app_sink = (GstAppSink*) obj;
    GstBuffer *buffer = gst_app_sink_pull_buffer(app_sink);
    em_audio_send(&em_audio, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));
}

static gboolean playbin2_bus_callback (GstBus *bus,GstMessage *message, gpointer data) {
  //g_print ("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR: {
      GError *err;
      gchar *debug;

      gst_message_parse_error (message, &err, &debug);
      g_print ("Error: %s\n", err->message);
      g_error_free (err);
      g_free (debug);

      g_main_loop_quit (loop);
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
    int len = strnlen(text, 255);
    int flags = 0;

    gst_init(&argc, &argv);


    GstElement *playbin2 = gst_element_factory_make("playbin2", "playbin2");
    GstAppSink *ledsink = GST_APP_SINK(gst_element_factory_make("appsink", "ledsink"));
    GstCaps *caps = gst_caps_from_string("video/x-raw-gray, width=448, height=240");

    GstBus *omnibus = gst_pipeline_get_bus(GST_PIPELINE(playbin2));
    if(!playbin2 || !ledsink || !caps){
        perror("Konnte Elemente nicht erzeugen\n");
        return -1;
    }
    if(!omnibus){
        perror("Konnte Bus nicht erzeugen\n");
        return -1;
    }
    gst_bus_add_watch (omnibus, playbin2_bus_callback, NULL);

    while (-1 != (c = getopt(argc, argv, "ae")))
	switch (c) {
	    case 'a':{
            flags |= MUTE_FLAG;
            break;
	    }

		case 'e':{
            flags |= EMBD_FLAG;
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


    g_object_set(G_OBJECT(ledsink), "caps", caps, NULL);
    g_object_set(playbin2, "video-sink", ledsink, NULL);
    //gst_app_sink_set_emit_signals (ledsink, TRUE);
    //g_signal_connect(ledsink, "new-buffer",  G_CALLBACK (on_new_buffer), NULL);

    ledsink_callbacks.eos = NULL;
    ledsink_callbacks.new_preroll = NULL;
    ledsink_callbacks.new_buffer = on_new_buffer;
    gst_app_sink_set_callbacks(ledsink, &ledsink_callbacks, NULL, NULL);

    GstAppSink *audio_sink = NULL;
    GstElement *audio_convert = NULL;

    if(flags & EMBD_FLAG){
        audio_sink  = GST_APP_SINK(gst_element_factory_make("appsink", "eaudiosink"));
        audio_convert = gst_element_factory_make("lamemp3enc", "audio_convert");
        if(!audio_sink && !audio_convert){
            g_error("Fuckup mit dem embd audio");
            return -1;
        }
        gst_element_link_many(audio_convert, GST_ELEMENT(audio_sink), NULL);

        if(em_audio_init(&em_audio)){
            g_error("Embedded Audio Fuckup!");
            return -1;
        }

        g_object_set(playbin2, "audio_sink", audio_convert, NULL);
        gst_app_sink_set_emit_signals(audio_sink, TRUE);
        g_signal_connect(audio_sink, "new-buffer", G_CALLBACK(on_new_audio_buffer), NULL);
    }


    ledwand_clear(&ledwand);
    ledwand_send(&ledwand, ASCII, 0, 0, len, 1, (uint8_t*)text, len);

    if (!flags & MUTE_FLAG){
        g_object_set(playbin2, "flags", 0x01, NULL);
    }

    g_object_set(playbin2, "uri", movie, NULL);
    loop = g_main_loop_new(NULL, FALSE);
    g_print("playing!\n");
    gst_element_set_state (playbin2, GST_STATE_PLAYING);
    g_main_loop_run(loop);

    // cleanup
    gst_element_set_state(playbin2, GST_STATE_NULL);
    close(ledwand.s_sock);
    gst_object_unref(GST_OBJECT(ledsink));

    if(flags & EMBD_FLAG){
        close(em_audio.s_sock);
        gst_object_unref(GST_OBJECT(audio_sink));
    }
    //ledwand_delete(&ledwand);
    //gst_object_unref(GST_OBJECT(omnibus));
    //gst_object_unref(GST_OBJECT(playbin2));
    //gst_object_unref(GST_OBJECT(loop));
    return 0;
}

