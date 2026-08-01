import arrowDownTray from "../assets/icons/arrow-down-tray.svg";
import arrowRight from "../assets/icons/arrow-right.svg";
import bars from "../assets/icons/bars-3.svg";
import codeBracket from "../assets/icons/code-bracket.svg";
import commandLine from "../assets/icons/command-line.svg";
import folder from "../assets/icons/folder.svg";
import github from "../assets/icons/github.svg";
import listBullet from "../assets/icons/list-bullet.svg";
import map from "../assets/icons/map.svg";
import close from "../assets/icons/x-mark.svg";

const iconSources = {
  download: arrowDownTray,
  arrow: arrowRight,
  code: codeBracket,
  terminal: commandLine,
  folder,
  github,
  list: listBullet,
  map,
  menu: bars,
  close,
} as const;

type IconName = keyof typeof iconSources;

type IconProps = {
  name: IconName;
  className?: string;
};

export function Icon({ name, className = "" }: IconProps) {
  return <img aria-hidden="true" alt="" className={`icon ${className}`} src={iconSources[name]} />;
}
