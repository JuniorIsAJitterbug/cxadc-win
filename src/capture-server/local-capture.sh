#!/usr/bin/env bash

set -euo pipefail

SCRIPT_NAME="$(basename "$0")"
SCRIPT_DIR="$(
	cd "$(dirname "$0")"
	pwd
)"
TEMP_DIR="$(mktemp --directory)"
SOCKET="$TEMP_DIR/server.sock"

# test the commands we need. if they fail, user will be appropriately notified by bash

jq --version >/dev/null
curl --version >/dev/null

if "$SCRIPT_DIR/cxadc_vhs_server" version &>/dev/null; then
	SERVER="$SCRIPT_DIR/cxadc_vhs_server"
fi

if "cxadc_vhs_server" version &>/dev/null; then
	SERVER="cxadc_vhs_server"
fi

if "$SCRIPT_DIR/ffmpeg" -version &>/dev/null; then
	FFMPEG_CMD="$SCRIPT_DIR/ffmpeg"
fi

if "ffmpeg" -version &>/dev/null; then
	FFMPEG_CMD="ffmpeg"
fi

if "$SCRIPT_DIR/flac" --version &>/dev/null; then
	FLAC_CMD="$SCRIPT_DIR/flac"
fi

if "flac" --version &>/dev/null; then
	FLAC_CMD="flac"
fi

if "$SCRIPT_DIR/sox" --version &>/dev/null; then
	SOX_CMD="$SCRIPT_DIR/sox"
fi

if "sox" --version &>/dev/null; then
	SOX_CMD="sox"
fi

if [[ -z "${SERVER-}" ]]; then
	echo "No working cxadc_vhs_server found."
fi

if [[ -z "${FFMPEG_CMD-}" ]]; then
	echo "No working ffmpeg found, some features may be unavailable. Obtain a binary here: https://johnvansickle.com/ffmpeg/"
fi

if [[ -z "${FLAC_CMD-}" ]]; then
	echo "No working flac found, some features may be unavailable."
fi

if [[ -z "${SOX_CMD-}" ]]; then
	echo "No working sox found, some features may be unavailable."
fi

for i in "$@"; do
	case $i in
	--video=*)
		CXADC_VIDEO="${i#*=}"
		shift # past argument=value
		;;
	--hifi=*)
		CXADC_HIFI="${i#*=}"
		shift # past argument=value
		;;
	--linear=*)
		LINEAR_DEVICE="${i#*=}"
		shift # past argument=value
		;;
	--linear-rate=*)
		LINEAR_RATE="${i#*=}"
		shift # past argument=value
		;;
	--add-date)
		ADD_DATE="YES"
		shift # past argument with no value
		;;
	--convert-linear)
		CONVERT_LINEAR="YES"
		shift # past argument with no value
		;;
	--compress-video)
		COMPRESS_VIDEO="YES"
		shift # past argument with no value
		;;
	--compress-video-level=*)
		COMPRESS_VIDEO_LEVEL="${i#*=}"
		shift # argument=value
		;;
	--compress-hifi)
		COMPRESS_HIFI="YES"
		shift # past argument with no value
		;;
	--compress-hifi-level=*)
		COMPRESS_HIFI_LEVEL="${i#*=}"
		shift # argument=value
		;;
	--resample-hifi)
		RESAMPLE_HIFI="YES"
		shift # past argument with no value
		;;
	--help)
		SHOW_HELP="YES"
		shift # past argument with no value
		;;
	--debug)
		set -x
		shift # past argument with no value
		;;
	-*)
		echo "Unknown option $i"
		exit 1
		;;
	*) ;;

	esac
done

function usage {
	echo "Usage: $SCRIPT_NAME [options] <basepath>" >&2
	printf "\t--video=                Number of CX card to use for video capture (unset=disabled)\n" >&2
	printf "\t--hifi=                 Number of CX card to use for hifi capture (unset=disabled)\n" >&2
	printf "\t--linear=               ALSA device identifier for linear (unset=default)\n" >&2
	printf "\t--linear-rate=          Linear sample rate (unset=46875)\n" >&2
	printf "\t--add-date              Add current date and time to the filenames\n" >&2
	printf "\t--convert-linear        Convert linear to flac+u8\n" >&2
	printf "\t--compress-video        Compress video\n" >&2
	printf "\t--compress-video-level  Video compression level (unset=4)\n" >&2
	printf "\t--compress-hifi         Compress hifi\n" >&2
	printf "\t--compress-hifi-level   Hifi compression level (unset=4)\n" >&2
	printf "\t--resample-hifi         Resample hifi to 10 MSps\n" >&2
	printf "\t--debug                 Show commands executed\n" >&2
	printf "\t--help                  Show usage information\n" >&2
	exit 1
}

if [[ -n "${SHOW_HELP-}" ]]; then
	usage
fi

if [[ -z "${1-}" ]]; then
	echo "Error: No path provided." >&2
	usage
fi

if [ -z "${FFMPEG_CMD-}" ]; then
	[ -n "${CONVERT_LINEAR-}" ] || echo "Converting linear requires ffmpeg." && exit 1
fi

if [ -z "${FLAC_CMD-}" ]; then
	[ -n "${COMPRESS_VIDEO-}" ] || echo "Compressing video requires flac." && exit 1
	[ -n "${COMPRESS_HIFI-}" ] || echo "Compressing hifi requires flac." && exit 1
fi

if [ -z "${SOX_CMD-}" ]; then
	[ -n "${RESAMPLE_HIFI-}" ] || echo "Resampling hifi requires sox." && exit 1
fi

OUTPUT_BASEPATH="$1"

if [[ -n "${ADD_DATE-}" ]]; then
	OUTPUT_BASEPATH="$OUTPUT_BASEPATH-$(date -Iseconds | sed 's/[T:\+]/_/g')"
fi

function die {
	echo "$@" >&2
	if [[ -n "${SERVER_PID-}" ]]; then
		kill "$SERVER_PID" || true
	fi
	if [[ -n "${VIDEO_PID-}" ]]; then
		kill "$VIDEO_PID" || true
	fi
	if [[ -n "${HIFI_PID-}" ]]; then
		kill "$HIFI_PID" || true
	fi
	if [[ -n "${LINEAR_PID-}" ]]; then
		kill "$LINEAR_PID" || true
	fi
	exit 1
}

"$SERVER" "unix:$SOCKET" &
SERVER_PID=$!

echo "Server started (PID $SERVER_PID)"

# wait for the server to listen
sleep 0.5

# test that the server runs
curl -X GET --unix-socket "$SOCKET" -s "http:/d/" >/dev/null || die "Server unreachable: $?"

CXADC_COUNTER=0
LINEAR_RATE=${LINEAR_RATE:=46875}

START_URL="http:/d/start?lrate=${LINEAR_RATE}&"
if [[ -n "${CXADC_VIDEO-}" ]]; then
	VIDEO_IDX="$CXADC_COUNTER"
	((CXADC_COUNTER += 1))
	START_URL="${START_URL}cxadc$CXADC_VIDEO&"
fi

if [[ -n "${CXADC_HIFI-}" ]]; then
	HIFI_IDX="$CXADC_COUNTER"
	((CXADC_COUNTER += 1))
	START_URL="${START_URL}cxadc$CXADC_HIFI&"
fi

if [[ -n "${LINEAR_DEVICE-}" ]]; then
	START_URL="${START_URL}lname=$LINEAR_DEVICE&"
fi

START_RESULT=$(curl -X GET --unix-socket "$SOCKET" -s "$START_URL" || die "Cannot send start request to server: $?")
echo "$START_RESULT" | jq -r .state | xargs test "Running" "=" || die "Server failed to start capture: $(echo "$START_RESULT" | jq -r .fail_reason)"
RET_LINEAR_RATE="$(echo "$START_RESULT" | jq -r .linear_rate)"

if [[ -n "${VIDEO_IDX-}" ]]; then
	if [[ -z "${COMPRESS_VIDEO-}" ]]; then
		VIDEO_PATH="$OUTPUT_BASEPATH-video.u8"
		curl -X GET --unix-socket "$SOCKET" -s --output "$VIDEO_PATH" "http:/d/cxadc?$VIDEO_IDX" &
	else
		VIDEO_PATH="$OUTPUT_BASEPATH-video.ldf"
		LEVEL="${COMPRESS_VIDEO_LEVEL:=4}" 
		curl -X GET --unix-socket "$SOCKET" -s --output - "http:/d/cxadc?$VIDEO_IDX" | "$FLAC_CMD" \
			--silent -"${LEVEL}" --blocksize=65535 --lax \
			--sample-rate=40000 --channels=1 --bps=8 \
			--sign=unsigned --endian=little \
			-f - -o "$VIDEO_PATH" &
	fi
	VIDEO_PID=$!
	echo "PID $VIDEO_PID is capturing video to $VIDEO_PATH"
fi
if [[ -n "${HIFI_IDX-}" ]]; then
	if [[ -z "${COMPRESS_HIFI-}" ]]; then
		HIFI_PATH="$OUTPUT_BASEPATH-hifi.u8"
		if [[ -z "${RESAMPLE_HIFI-}" ]]; then
			curl -X GET --unix-socket "$SOCKET" -s --output "$HIFI_PATH" "http:/d/cxadc?$HIFI_IDX" &
		else
			curl -X GET --unix-socket "$SOCKET" -s --output - "http:/d/cxadc?$HIFI_IDX" | "$SOX_CMD" \
				-D \
				-t raw -r 400000 -b 8 -c 1 -L -e unsigned-integer - \
				-t raw           -b 8 -c 1 -L -e unsigned-integer "$HIFI_PATH" rate -l 100000 &
		fi
	else
		HIFI_PATH="$OUTPUT_BASEPATH-hifi.flac"
		LEVEL="${COMPRESS_HIFI_LEVEL:=4}" 
		if [[ -z "${RESAMPLE_HIFI-}" ]]; then
			curl -X GET --unix-socket "$SOCKET" -s --output - "http:/d/cxadc?$HIFI_IDX" | "$FLAC_CMD" \
				--silent -"${LEVEL}" --blocksize=65535 --lax \
				--sample-rate=40000 --channels=1 --bps=8 \
				--sign=unsigned --endian=little \
				-f - -o "$HIFI_PATH" &
		else
			curl -X GET --unix-socket "$SOCKET" -s --output - "http:/d/cxadc?$HIFI_IDX" | "$SOX_CMD" \
				-D \
				-t raw -r 400000 -b 8 -c 1 -L -e unsigned-integer - \
				-t raw           -b 8 -c 1 -L -e unsigned-integer - rate -l 100000 | "$FLAC_CMD" \
					--silent -"${LEVEL}" --blocksize=65535 --lax \
					--sample-rate=10000 --channels=1 --bps=8 \
					--sign=unsigned --endian=little \
					-f - -o "$HIFI_PATH" &
		fi
	fi
	HIFI_PID=$!
	echo "PID $HIFI_PID is capturing hifi to $HIFI_PATH"
fi

if [[ -n "${CONVERT_LINEAR-}" ]]; then
	HEADSWITCH_PATH="$OUTPUT_BASEPATH-headswitch.u8"
	LINEAR_PATH="$OUTPUT_BASEPATH-linear.flac"
	curl -X GET --unix-socket "$SOCKET" -s --output - "http:/d/linear" | "$FFMPEG_CMD" \
		-hide_banner -loglevel error \
		-ar "$RET_LINEAR_RATE" -ac 3 -f s24le -i - \
		-filter_complex "[0:a]channelsplit=channel_layout=2.1[FL][FR][headswitch],[FL][FR]amerge=inputs=2[linear]" \
		-map "[linear]" -compression_level 0 "$LINEAR_PATH" \
		-map "[headswitch]" -f u8 "$HEADSWITCH_PATH" &
	LINEAR_PID=$!
	echo "PID $LINEAR_PID is capturing linear to $LINEAR_PATH, headswitch to $HEADSWITCH_PATH"
else
	LINEAR_PATH="$OUTPUT_BASEPATH-linear.s24"
	curl -X GET --unix-socket "$SOCKET" -s --output "$LINEAR_PATH" "http:/d/linear" &
	LINEAR_PID=$!
	echo "PID $LINEAR_PID is capturing linear+headswitch to $LINEAR_PATH"
fi

SECONDS=0

echo "Capture running... Press 'q' to stop the capture."

while true; do
	ELAPSED=$SECONDS
	STATS=$(curl -X GET --unix-socket "$SOCKET" -s --output - "http:/d/stats" || true);
	if [[ -z "${STATS-}" ]]; then
		STATS_MSG="Failed to get stats."
	else
		STATS_MSG="Buffers: $(echo "$STATS" | jq .linear.difference_pct | xargs printf '% 2s%% ')$(echo "$STATS" | jq .cxadc[].difference_pct | xargs printf '% 2s%% ')"
	fi
	echo "Capturing for $((ELAPSED / 60))m $((ELAPSED % 60))s... $STATS_MSG"
	if read -r -t 5 -n 1 key; then
		if [[ $key == "q" ]]; then
			echo -e "\nStopping capture"
			break
		else
			echo -e "\nPress 'q' to stop the capture."
		fi
	fi
done

STOP_RESULT=$(curl -X GET --unix-socket "$SOCKET" -s "http:/d/stop" || die "Cannot send stop request to server: $?")
echo "$STOP_RESULT" | jq -r .state | xargs test "Idle" "=" || die "Server failed to stop capture: ${STOP_RESULT}"
echo "$STOP_RESULT" | jq -r .overflows | xargs printf "Encountered %d overflows during capture\n" || die "Can't find overflow information"

echo "Waiting for writes to finish..."

for PID in ${VIDEO_PID-} ${HIFI_PID-} $LINEAR_PID; do
	wait "$PID" || true
done

echo "Killing server"

kill $SERVER_PID

echo "Finished!"
