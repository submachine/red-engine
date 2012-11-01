#!/bin/bash

# Create a DB containing a couple of redirects

echo "Creating test database."

mkdir -p $TEST_HOME
rm -f $TEST_HOME/__db*
rm -f $TEST_HOME/*.db

echo -e "/goo.gl\nhttp://www.google.com\n/gnu\nhttp://www.gnu.org\n" \
| db_load -T -t btree $TEST_HOME/red-engine.db \

# Choose a random free port

echo "Looking for a free port to use."

FOUND_FREE_PORT=0
while [ "$FOUND_FREE_PORT" -eq "0" ]; do

  # Randomize
  let PORT=49152+$RANDOM%10000

  # Make sure it is un-used
  netstat -ant \
  | sed -e '/^tcp/ !d' \
        -e 's/^[^ ]* *[^ ]* *[^ ]* *.*[\.:]\([0-9]*\).*$/\1/' \
  | sort -g \
  | uniq \
  | grep $PORT

  let FOUND_FREE_PORT=$?
done

# Execute red-engine and wait for it to set-up

echo "Executing: $TEST_BINARY --home-dir=$TEST_HOME --port=$PORT"
$TEST_BINARY --home-dir=$TEST_HOME --port=$PORT && sleep 2

# Test 1
# Verify that it is running

PID=$(pidof $TEST_BINARY)
if [ $(kill -s 0 $PID) ]; then
  echo "Failure: $TEST_BINARY did not start."
  exit 1
fi

echo "Success: $TEST_BINARY is running."

# Function for quitting on success/failure
quit_test () {

  # Kill red-engine
  echo "Executing: kill $PID"
  kill $PID && sleep 2

  # Test
  # Has it quit?
  if [ $(kill -s 0 $PID &> /dev/null) ]; then
    echo "Failure: $TEST_BINARY did not exit. Forcing."
    kill -s 9 $PID
    exit 1
  else
    echo "Success: $TEST_BINARY exited."
  fi

  if [ "$1" -eq "0" ]; then
    echo "All tests successful."
  else
    echo "Some tests failed."
  fi

  exit $1
}

# Test
# HTTP 405 Method Not Allowed

echo "Executing: wget -q -S -O - --post-data="foo" http://localhost:$PORT"
wget -q -S -O - --post-data="foo" http://localhost:$PORT 2>&1 \
| grep 405 &> /dev/null

if [ "$?" -eq "0" ]; then
  echo "Success: Got HTTP 405"
else
  echo "Failure"
  quit_test 1
fi

# Test
# HTTP 404 Not Found
echo "Executing: wget -q -S -O - http://localhost:$PORT/foo"
wget -q -S -O - http://localhost:$PORT/foo 2>&1 \
| grep 404 &> /dev/null

if [ "$?" -eq "0" ]; then
  echo "Success: Got HTTP 404"
else
  echo "Failure"
  quit_test 1
fi

# Test
# HTTP 301 Moved Permanently
echo "Executing: wget -q -S -O - --max-redirect=0 http://localhost:$PORT/goo.gl"
wget -q -S -O - --max-redirect=0 http://localhost:$PORT/goo.gl 2>&1 \
| grep 301 &> /dev/null

if [ "$?" -eq "0" ]; then
  echo "Success: Got HTTP 301"
else
  echo "Failure"
  quit_test 1
fi

quit_test 0
