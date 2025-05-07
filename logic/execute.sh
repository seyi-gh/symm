#!bin/bash
command="npm run run"

# run this command 10 times in a for loop
for i in {1..10}
do
  echo "Running command $i: $command"
  $command &

  if [ $? -eq 0 ]; then
    echo "Command ran successfully"
  else
    echo "Command failed"
  fi
done