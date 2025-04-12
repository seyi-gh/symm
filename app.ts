import chalk from "chalk";
import proxyController from "./conn";

const mainApp = proxyController('/');
mainApp.listen(4001, () => {
  console.log(chalk.cyan('<4001> [app.main] connected'));
});

const subApp = proxyController('/');
subApp.listen(4002, () => {
  console.log(chalk.cyan('<4002> [app.sub] connected'));
});