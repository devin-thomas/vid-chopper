import type { ReactNode } from "react";

type SectionHeadingProps = {
  title: string;
  body: string;
  aside?: ReactNode;
};

export function SectionHeading({ title, body, aside }: SectionHeadingProps) {
  return (
    <div className="section-heading">
      <div>
        <h2>{title}</h2>
        <p>{body}</p>
      </div>
      {aside ? <div className="section-aside">{aside}</div> : null}
    </div>
  );
}
