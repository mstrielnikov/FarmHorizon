declare namespace Bun {
  interface BunFile extends Blob {
    exists(): Promise<boolean>;
  }
  function file(path: string): BunFile;
  function serve(options: {
    port: number;
    fetch(req: Request): Response | Promise<Response>;
  }): { port: number };
}
