const fps = 60.0; //used for computing timestamps

let ts = 0;
const num_iter = 10;
for (let iter = 0; iter < num_iter; ++iter) {
	console.log(`${Math.round(ts)} MARK iteration ${iter+1} of ${num_iter}`);
	console.log(`${Math.round(ts)} PLAY 0 1`);
	for (let frame = 0; frame < 120; ++frame) {
		console.log(`${Math.round(ts)} AVAILABLE`);
		if ((frame+1) % 30 == 0) {
			console.log(`${Math.round(ts)} SAVE iter-${iter+1}-frame-${frame+1}.ppm`);
		}
		ts += (1.0 / fps) * 1e6;
	}
}