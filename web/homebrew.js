import {games} from './homebrew/catalog.js';
export {games};

export async function fetchHomebrew(id, fetcher=fetch) {
    const game=games.find(g=>g.id===id);
    if(!game)throw new Error('Unknown homebrew cartridge.');
    const response=await fetcher(game.rom);
    if(!response.ok)throw new Error('Game download failed. Please try again.');
    const bytes=new Uint8Array(await response.arrayBuffer());
    const hash=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),n=>n.toString(16).padStart(2,'0')).join('');
    if(hash!==game.sha256)throw new Error('Game download is incomplete or changed. Please reload and try again.');
    return {game,bytes};
}
