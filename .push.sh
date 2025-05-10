#!bin/bash

rm ./*.o ./*/*.o

git add .
read -p "[?] Commit message" commit_message
git commit -m "$commit_message"
git push origin main

echo "[✓] Pushed to GitHub"